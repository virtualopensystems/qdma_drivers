/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : ptdr_dev.c
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description : PTDR device driver implementation
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>

#include "ptdr_dev.h"
#include "ptdr_regs.h"
#include "qdma_queues.h"

typedef struct {
    uint64_t __sign;
    uint64_t base;
    struct queue_info *q_info;
} ptdr_dev_t;

#define REG_SIZE    (4) //size of registers in bytes
#define PTDR_MAGIC  ((uint64_t) 0xC001C0DE50544452ULL)

// Check device pointer, return -EINVAL if invalid
#define CHECK_DEV_PTR(dev) do { \
    if ((dev == NULL) || \
            (((ptdr_dev_t*)dev)->q_info == NULL) || \
            (((ptdr_dev_t*)dev)->__sign != PTDR_MAGIC) ) \
    { \
        fprintf(stderr, "ERR: invalid dev pointer\n"); \
        return -EINVAL; \
    } \
} while (0)


/* Additional debug prints  */
#ifdef DEBUG_DEV
#define debug_print(format, ...)    printf("  [PTDR_DEV] " format, ## __VA_ARGS__)
#else
#define debug_print(format, ...)    do { } while (0)
#endif

#ifndef MAX_SIZE_ID
#define MAX_SIZE_ID 32ULL
#endif
#ifndef MAX_SIZE_SEGMENTS
#define MAX_SIZE_SEGMENTS 160ULL
#endif
#ifndef PROFILES_NUM
#define PROFILES_NUM 672ULL
#endif
#ifndef PROFILE_VAL_NUM
#define PROFILE_VAL_NUM 4ULL
#endif


// Structure used to convert an etl::vector_t type to a C array
// This structure should preceed an array that was defined as vector_t
struct vec_conv {
    uint64_t max;
    uint64_t z;
    uint64_t size;
};

// Probability profile for a single segment.
// This profile is sampled to determine the Level of Service (how easy is to go through the segment).
struct segment_time_profile {
    double values[PROFILE_VAL_NUM];
    double cum_probs[PROFILE_VAL_NUM];
};

// A single segment of a road.
struct segment {
#ifndef STATIC
    char id[MAX_SIZE_ID];
#endif
    double length; // How precise is distance? mm, cm, meters? What is the maximum length of a segment?
    double speed; // How precise is speed? mm/s, cm/s, m/s?
};

// Wrapper struct that contains segment data and its probability profile.
struct enriched_segment {
    struct segment segment;
    struct segment_time_profile profiles[PROFILES_NUM];
};

// Single route which will be sampled using Monte Carlo to determine how long would it take to go through it.
typedef struct {
    // Duration of an atomic movement of a car on a segment.
    double frequency_seconds;
    struct vec_conv segments_vec;
    struct enriched_segment segments[MAX_SIZE_SEGMENTS];
} ptdr_route_t;

typedef struct {
    // Index of a specific segment on which the car is currently located.
    unsigned long long segment_index;
    // Number in the range [0.0, 1.0] which determines how far is the car along the segment.
    double progress; // How precise should this be? Could this be converted to int? Like in [0,100] range?
} ptdr_routepos_t;

//typedef unsigned long long ptdr_datetime_t; //unused
//typedef unsigned long long ptdr_duration_t; //unused
//typedef const unsigned long long ptdr_seed_t; //unused


static inline int ptdr_reg_read(ptdr_dev_t *dev, uint32_t *data, uint16_t reg)
{
    return queue_read(dev->q_info, data, (uint64_t) REG_SIZE, (uint64_t) dev->base + reg) != REG_SIZE;
}

static inline uint64_t ptdr_reg_write(ptdr_dev_t *dev, uint32_t data, uint16_t reg)
{
    return queue_write(dev->q_info, &data, (uint64_t) REG_SIZE, (uint64_t) dev->base + reg) != REG_SIZE;
}

int ptdr_dev_destroy(void* dev)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    ptdr->__sign = 0;

    debug_print("In %s: destroy queue for ptdr dev\n", __func__);
    (void) queue_destroy(ptdr->q_info);
    free(ptdr);

    return 0;
}

void* ptdr_dev_init(uint64_t dev_addr, int pci_bus, int pci_dev, int fun_id, int is_vf, int q_start)
{
    int ret;
    ptdr_dev_t *ptdr;
    struct queue_conf q_conf;
    uint32_t data;

    ptdr = (ptdr_dev_t*) malloc(sizeof(ptdr_dev_t));
    if (ptdr == NULL) {
        fprintf(stderr, "ERR: Cannot allocate %ld bytes\n", sizeof(ptdr_dev_t));
        return NULL;
    }

    q_conf.pci_bus = pci_bus;
    q_conf.pci_dev = pci_dev;
    q_conf.fun_id = fun_id;
    q_conf.is_vf = is_vf;
    q_conf.q_start = q_start;

    debug_print("In %s: setup queue for ptdr dev\n", __func__);
    ret = queue_setup(&ptdr->q_info, &q_conf);
    if (ret < 0) {
        free(ptdr);
        return NULL;
    }

    ptdr->base = dev_addr;
    debug_print("In %s: setup done, base addr 0x%016lx\n", __func__, ptdr->base);

    // Test if kernel control register is readable
    if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
        fprintf(stderr, "ERR: Cannot access ptdr device @ 0x%016lx\n", dev_addr);
        ptdr_dev_destroy((void*)ptdr);
        return NULL;
    }

    ptdr->__sign = PTDR_MAGIC;

    return (void*) ptdr;
}

static int ptdr_read_route_from_file(char *filename, ptdr_route_t *route)
{
    uint64_t count;
    size_t rsize = 0;
    FILE* file = fopen(filename, "rb");

    if (file == NULL) {
        fprintf(stderr, "ERR %d: Failed opening file \"%s\"\n", errno, filename);
        return -ENOENT;
    }

    if ((rsize = fread(&route->frequency_seconds, 1, sizeof(double), file)) != sizeof(double)) goto read_err;
    debug_print("  Frequency %f\n", route->frequency_seconds);

    if ((rsize = fread(&count, 1, sizeof(uint64_t), file)) != sizeof(uint64_t)) goto read_err;
    debug_print("  Segments 0x%08lx %ld\n", count, count);

    if (count > MAX_SIZE_SEGMENTS) {
        fprintf(stderr, "ERR: Invalid Segments %ld > MAX_SIZE_SEGMENTS %lld\n", count, MAX_SIZE_SEGMENTS);
        fclose(file);
        return -EINVAL;
    }

    route->segments_vec.max = MAX_SIZE_SEGMENTS;
    route->segments_vec.z = 0;
    route->segments_vec.size = count;

    // Iterate on each segment
    for (int i = 0; i < count; i++) {

        // Ignore the ID, it is not needed to be loaded into memory
        uint64_t id_num;
        if ((rsize = fread(&id_num, 1, sizeof(uint64_t), file)) != sizeof(uint64_t)) goto read_err;
        //debug_print("Ignoring ID_num 0x%08lx %ld\n", id_num, id_num);
        if (fseek(file, id_num, SEEK_CUR) != 0) {
            fprintf(stderr, "ERR %d: Failed fseek on file \"%s\"\n", errno, filename);
            fclose(file);
            return -EIO;
        }

        if ((rsize = fread(&route->segments[i].segment.length, 1, sizeof(double), file)) != sizeof(double)) goto read_err;

        if ((rsize = fread(&route->segments[i].segment.speed, 1, sizeof(double), file)) != sizeof(double)) goto read_err;

        for (int j = 0; j < PROFILES_NUM; j++) {
            for(int k = 0; k < PROFILE_VAL_NUM; k++) {
                if ((rsize = fread(&route->segments[i].profiles[j].values[k], 1, sizeof(double), file)) != sizeof(double)) goto read_err;
            }
            for(int k = 0; k < PROFILE_VAL_NUM; k++) {
                if ((rsize = fread(&route->segments[i].profiles[j].cum_probs[k], 1, sizeof(double), file)) != sizeof(double)) goto read_err;
            }
        }
    }

    debug_print("In %s, read 0x%lx bytes\n", __func__, ftell(file));

    fclose(file);
    return 0;

read_err:
    fprintf(stderr, "ERR: read only %ld bytes\n", rsize);
    fclose(file);
    return -EIO;
}

int ptdr_dev_conf(void* dev, char* route_file, uint64_t *duration_v,
        uint64_t samples_count, uint64_t routepos_index,
        uint64_t routepos_progress, uint64_t departure_time,
        uint64_t seed, uint64_t base, uint64_t end)
{
    int ret = 0;
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    CHECK_DEV_PTR(dev);
    ptdr_route_t route = {0};
    ptdr_routepos_t start_pos = {routepos_index, routepos_progress};

    uint64_t ptdr_data_size = sizeof(struct vec_conv) + samples_count * sizeof(duration_v[0]) +
        sizeof(route) + sizeof(start_pos) + sizeof(departure_time) + sizeof(seed);

    debug_print("Config data size 0x%lx, mem avail 0x%lx\n", ptdr_data_size, end-base);
    if (ptdr_data_size > (end - base)) {
        fprintf(stderr, "VF does not have enough space (needed %ld, available %ld bytes)\n", ptdr_data_size, end-base);
        return -ENOMEM;
    }

    ret = ptdr_read_route_from_file(route_file, &route);
    if (ret != 0) {
        fprintf(stderr, "ERR %d reading route from file \"%s\"\n", ret, route_file);
        return ret;
    }

    uint64_t ptr = 0;

    // Write duration structure to memory (starting from base addr) and set ptr into register
    {
        struct vec_conv dur_vc = {samples_count, 0, samples_count};
        if (queue_write(ptdr->q_info, &dur_vc, sizeof(dur_vc), base + ptr) != sizeof(dur_vc)) return -EIO;
        ptr += sizeof(dur_vc);

        uint64_t duration_size = samples_count * sizeof(duration_v[0]);
        if (queue_write(ptdr->q_info, duration_v, duration_size, base + ptr) != duration_size) return -EIO;

        // Duration start at 0, including the conversion vector
        if ((ret = ptdr_set_durations(dev, 0)) != 0) return ret;
        debug_print("DUR     @0x%015x %d\n", 0, 0);
        ptr += duration_size;
    }

    // Write route structure to memory (after duration) and set ptr into register
    if (queue_write(ptdr->q_info, &route, sizeof(route), base + ptr) != sizeof(route)) return -EIO;
    if ((ret = ptdr_set_route(dev, ptr)) != 0) return ret;
    debug_print("ROUTE   @0x%015lx %ld\n", ptr, ptr);
    ptr += sizeof(route);

    // Write start_pos structure to memory (after route) and set ptr into register
    if (queue_write(ptdr->q_info, &start_pos, sizeof(start_pos), base + ptr) != sizeof(start_pos)) return -EIO;
    if ((ret = ptdr_set_position(dev, ptr)) != 0) return ret;
    debug_print("STARTP  @0x%015lx %ld\n", ptr, ptr);
    ptr += sizeof(start_pos);

    // Write departure_time to memory (after start pos) and set ptr into register
    if (queue_write(ptdr->q_info, &departure_time, sizeof(departure_time), base + ptr) != sizeof(departure_time)) return -EIO;
    if ((ret = ptdr_set_departure(dev, ptr)) != 0) return ret;
    debug_print("DEPTIME @0x%015lx %ld\n", ptr, ptr);
    ptr += sizeof(departure_time);

    // Write seed to memory (after departure) and set ptr into register
    if (queue_write(ptdr->q_info, &seed, sizeof(seed), base + ptr) != sizeof(seed)) return -EIO;
    if ((ret = ptdr_set_seed(dev, ptr)) != 0) return ret;
    debug_print("SEED    @0x%015lx %ld\n", ptr, ptr);

    debug_print("\n\nS dur %ld route %ld pos %ld dep %ld seed %ld, tot %ld (0x%lx)\n",
                 samples_count*sizeof(uint64_t)+24, sizeof(route), sizeof(start_pos), sizeof(departure_time), sizeof(seed),
                 ptr + sizeof(seed), ptr + sizeof(seed));

    // Set base register
    if ((ret = ptdr_set_base(dev, base)) != 0) return ret;

    return 0;
}

int ptdr_dev_get_durv(void* dev, uint64_t *duration_v, uint64_t samples_count, uint64_t base)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    CHECK_DEV_PTR(dev);

    uint64_t ptr = 0;
    struct vec_conv dur_vc = {samples_count, 0, samples_count};
    if (queue_read(ptdr->q_info, &dur_vc, sizeof(dur_vc), base + ptr) != sizeof(dur_vc)) return -EIO;
    ptr += sizeof(dur_vc);


    if (dur_vc.size != samples_count) {
        fprintf(stderr, "ERR: got %ld samples, expected %ld\n", dur_vc.size, samples_count);
        return -EINVAL;
    }

    uint64_t duration_size = samples_count * sizeof(duration_v[0]);
    if (queue_read(ptdr->q_info, duration_v, duration_size, base + ptr) != duration_size) return -EIO;

    return 0;
}

int ptdr_start(void *dev)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    uint32_t data;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
        return -EIO;
    }
    debug_print("In %s: CTRL reg is 0x%08x\n", __func__, data);

    if (data & 0x01) {
        // Not a fatal error
        debug_print("In %s: kernel is not ready! (ctrl reg is 0x%08x)", __func__, data);
        return -EBUSY;
    }

    data &= 0x80; //keep only auto_restart bit
    data |= 0x01; //set ap_start bit

    debug_print("setting CTRL reg to 0x%08x\n", data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_CTRL)) {
        return -EIO;
    }

    return 0;
}

int ptdr_isdone(void *dev)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    uint32_t data;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
        return -EIO;
    }
    //debug_print("In %s: CTRL reg is 0x%08x, done is %d\n",
    //  __func__, data, (data >> 1) & 0x01);

    // ap_done is BIT(1)
    return (data >> 1) & 0x01;
}

int ptdr_isidle(void *dev)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    uint32_t data;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
        return -EIO;
    }
    //debug_print("In %s: CTRL reg is 0x%08x, idle is %d\n",
    //  __func__, data, (data >> 2) & 0x01);

    // ap_idle is BIT(2)
    return (data >> 2) & 0x01;
}

int ptdr_isready(void *dev)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    uint32_t data;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
        return -EIO;
    }
    //debug_print("In %s: CTRL reg is 0x%08x, ready is %d\n",
    //  __func__, data, (data >> 3) & 0x01);

    // Do not check ready bit (BIT 3), check ap_start == 0 to see if the kernel is ready for next input
    return !(data & 0x01);
}

int ptdr_continue(void *dev)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    uint32_t data;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
        return -EIO;
    }
    debug_print("In %s: CTRL reg is 0x%08x", __func__, data);

    data &= 0x80; //keep only auto_restart bit
    data |= 0x10; //set ap_continue bit

    debug_print("  writing 0x%08x\n", data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_CTRL)) {
        return -EIO;
    }

    return 0;
}

int ptdr_autorestart(void *dev, int enable)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    uint32_t data;

    CHECK_DEV_PTR(dev);

    // Write 0x80 to enable, 0 to disable autorestart
    data = (enable == 0) ? 0 : 0x80;

    debug_print("In %s: writing 0x%08x to CTRL reg\n", __func__, data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_CTRL)) {
        return -EIO;
    }

    return 0;
}

int ptdr_set_numtimes(void *dev, uint32_t data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    debug_print("In %s: writing 0x%08x to NUM_TIMES reg\n", __func__, data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_NUM_TIMES)) {
        return -EIO;
    }

    return 0;
}

int ptdr_get_numtimes(void *dev, uint32_t *data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_NUM_TIMES)) {
        return -EIO;
    }
    debug_print("In %s: NUM_TIMES reg is 0x%08x\n", __func__, *data);

    return 0;
}

int ptdr_set_durations(void *dev, uint32_t data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    debug_print("In %s: writing 0x%08x to DUR reg\n", __func__, data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_DUR)) {
        return -EIO;
    }

    return 0;
}

int ptdr_get_durations(void *dev, uint32_t *data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_DUR)) {
        return -EIO;
    }
    debug_print("In %s: DUR reg is 0x%08x\n", __func__, *data);

    return 0;
}

int ptdr_set_route(void *dev, uint32_t data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    debug_print("In %s: writing 0x%08x to ROUTE reg\n", __func__, data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_ROUTE)) {
        return -EIO;
    }

    return 0;
}

int ptdr_get_route(void *dev, uint32_t *data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_ROUTE)) {
        return -EIO;
    }
    debug_print("In %s: ROUTE reg is 0x%08x\n", __func__, *data);

    return 0;
}

int ptdr_set_position(void *dev, uint32_t data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    debug_print("In %s: writing 0x%08x to POS reg\n", __func__, data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_POS)) {
        return -EIO;
    }

    return 0;
}

int ptdr_get_position(void *dev, uint32_t *data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_POS)) {
        return -EIO;
    }
    debug_print("In %s: POS reg is 0x%08x\n", __func__, *data);

    return 0;
}

int ptdr_set_departure(void *dev, uint32_t data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    debug_print("In %s: writing 0x%08x to DEP reg\n", __func__, data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_DEP)) {
        return -EIO;
    }

    return 0;
}

int ptdr_get_departure(void *dev, uint32_t *data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_DEP)) {
        return -EIO;
    }
    debug_print("In %s: DEP reg is 0x%08x\n", __func__, *data);

    return 0;
}

int ptdr_set_seed(void *dev, uint32_t data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    debug_print("In %s: writing 0x%08x to SEED reg\n", __func__, data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_SEED)) {
        return -EIO;
    }

    return 0;
}

int ptdr_get_seed(void *dev, uint32_t *data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_SEED)) {
        return -EIO;
    }
    debug_print("In %s: SEED reg is 0x%08x\n", __func__, *data);

    return 0;
}

int ptdr_set_base(void *dev, uint64_t data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    debug_print("In %s: writing 0x%08x to BASE[0] reg\n", __func__, (uint32_t) data);
    if (ptdr_reg_write(ptdr, (uint32_t) data, PTDR_CTRL_ADDR_BASE)) {
        return -EIO;
    }

    debug_print("In %s: writing 0x%08x to BASE[1] reg\n", __func__, (uint32_t) (data >> 32));
    if (ptdr_reg_write(ptdr, (uint32_t) (data >> 32) , PTDR_CTRL_ADDR_BASE + REG_SIZE)) {
        return -EIO;
    }

    return 0;
}

int ptdr_get_base(void *dev, uint64_t *data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    uint32_t data0, data1;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, &data0, PTDR_CTRL_ADDR_BASE)) {
        return -EIO;
    }
    debug_print("In %s: BASE[0] reg is 0x%08x\n", __func__, data0);

    if (ptdr_reg_read(ptdr, &data1, PTDR_CTRL_ADDR_BASE + REG_SIZE)) {
        return -EIO;
    }
    debug_print("In %s: BASE[1] reg is 0x%08x\n", __func__, data1);

    *data = ((uint64_t) data0) | ((uint64_t) data1 << 32);
    debug_print("In %s: BASE[0-1] reg is 0x%016lx\n", __func__, *data);

    return 0;
}

int ptdr_interruptglobal(void *dev, int enable)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    uint32_t data;

    CHECK_DEV_PTR(dev);

    // Write 0x01 to enable, 0 to disable interrupt global
    data = (enable == 0) ? 0 : 0x01;

    debug_print("In %s: writing 0x%08x to GIE reg\n", __func__, data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_GIE)) {
        return -EIO;
    }

    return 0;
}

int ptdr_set_interruptconf(void *dev, uint32_t data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    debug_print("In %s: writing 0x%08x to IER reg\n", __func__, data);
    if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_IER)) {
        return -EIO;
    }

    return 0;
}

int ptdr_get_interruptconf(void *dev, uint32_t *data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_IER)) {
        return -EIO;
    }
    debug_print("In %s: IER reg is 0x%08x\n", __func__, *data);

    return 0;
}

int ptdr_get_interruptstatus(void *dev, uint32_t *data)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

    CHECK_DEV_PTR(dev);

    // Current Interrupt Clear Behavior is Clear on Read(COR).
    if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_ISR)) {
        return -EIO;
    }
    debug_print("In %s: ISR reg is 0x%08x\n", __func__, *data);

    return 0;
}

ssize_t ptdr_mem_write(void *dev, void* data, size_t size, uint64_t mem_addr) {
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    CHECK_DEV_PTR(dev);

    return queue_write(ptdr->q_info, data, size, mem_addr);
}

ssize_t ptdr_mem_read(void *dev, void* data, size_t size, uint64_t mem_addr) {
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    CHECK_DEV_PTR(dev);

    return queue_read(ptdr->q_info, data, size, mem_addr);
}

// For debug only
#ifdef DEBUG
int ptdr_reg_dump(void *dev)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    uint32_t data = 0;

    CHECK_DEV_PTR(dev);

    printf("\nIn %s: Dumping device registers @ 0x%016lx\n", __func__, ptdr->base);

    (void) ptdr_ctrl_dump(dev);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_GIE);
    printf("  0x%02x GIE:    0x%08x\n", PTDR_CTRL_ADDR_GIE, data);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_IER);
    printf("  0x%02x IER:    0x%08x\n", PTDR_CTRL_ADDR_IER, data);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_ISR);
    printf("  0x%02x ISR:    0x%08x\n", PTDR_CTRL_ADDR_ISR, data);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_NUM_TIMES);
    printf("  0x%02x NUM:    0x%08x\n", PTDR_CTRL_ADDR_NUM_TIMES, data);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_DUR);
    printf("  0x%02x DUR:    0x%08x\n", PTDR_CTRL_ADDR_DUR, data);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_ROUTE);
    printf("  0x%02x ROUTE:  0x%08x\n", PTDR_CTRL_ADDR_ROUTE, data);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_POS);
    printf("  0x%02x POS:    0x%08x\n", PTDR_CTRL_ADDR_POS, data);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_DEP);
    printf("  0x%02x DEP:    0x%08x\n", PTDR_CTRL_ADDR_DEP, data);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_SEED);
    printf("  0x%02x SEED:   0x%08x\n", PTDR_CTRL_ADDR_SEED, data);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_BASE);
    printf("  0x%02x BASE0:  0x%08x\n", PTDR_CTRL_ADDR_BASE, data);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_BASE + REG_SIZE);
    printf("  0x%02x BASE1:  0x%08x\n", PTDR_CTRL_ADDR_BASE + REG_SIZE, data);

    return 0;
}

int ptdr_ctrl_dump(void *dev)
{
    ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
    uint32_t data = 0;

    CHECK_DEV_PTR(dev);

    (void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL);
    printf("  0x%02x CTRL:   0x%08x ", PTDR_CTRL_ADDR_CTRL, data);
    printf(" start %d", (data >> 0) & 0x01);
    printf(" done %d", (data >> 1) & 0x01);
    printf(" idle %d", (data >> 2) & 0x01);
    printf(" ready %d", (data >> 3) & 0x01);
    printf(" cont %d", (data >> 4) & 0x01);
    printf(" rest %d", (data >> 7) & 0x01);
    printf(" inter %d\n", (data >> 9) & 0x01);

    return 0;
}
#endif
