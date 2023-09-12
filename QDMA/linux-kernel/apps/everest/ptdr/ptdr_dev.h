/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : ptdr_dev.h
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description :
 *
 */

#ifndef PTDR_DEV_H
#define PTDR_DEV_H

#if defined(__BAMBU__) && !defined(STATIC)
#define STATIC
#endif

#define PTDR_AP_DONE_INTERRUPT 		(1 << 0)
#define PTDR_AP_READY_INTERRUPT 	(1 << 1)

#ifndef MAX_SIZE_ID
#define MAX_SIZE_ID 32ULL
#endif

#ifndef MAX_SIZE_SEGMENTS
#define MAX_SIZE_SEGMENTS 160ULL
#endif

#define PROFILES_NUM 672ULL
#define PROFILE_VAL_NUM 4ULL


// Probability profile for a single segment.
// This profile is sampled to determine the Level of Service (how easy is to go through the segment).
struct segment_time_profile {
	double values[PROFILE_VAL_NUM];
	double cum_probs[PROFILE_VAL_NUM];
};

// A single segment of a road.
struct segment {
#ifndef STATIC
	//string_t<MAX_SIZE_ID> id; // Is this necessary? Can this be converted to a fixed size char array? Or even removed?
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
	//vector_t<EnrichedSegment, MAX_SIZE_SEGMENTS> segments;
	struct enriched_segment segments[MAX_SIZE_SEGMENTS];
} ptdr_route_t;

typedef struct {
	// Index of a specific segment on which the car is currently located.
	unsigned long long segment_index;
	// Number in the range [0.0, 1.0] which determines how far is the car along the segment.
	double progress; // How precise should this be? Could this be converted to int? Like in [0,100] range?
} ptdr_routepos_t;

typedef unsigned long long ptdr_datetime_t;
typedef unsigned long long ptdr_duration_t;
typedef const unsigned long long ptdr_seed_t;


void* ptdr_dev_init(uint64_t dev_addr, int pci_bus, int pci_dev, int fun_id, int is_vf, int q_start);

int ptdr_dev_destroy(void* dev);

int ptdr_start(void *dev);

int ptdr_isdone(void *dev);

int ptdr_isidle(void *dev);

int ptdr_isready(void *dev);

int ptdr_continue(void *dev);

int ptdr_autorestart(void *dev, int enable);

int ptdr_set_numtimes(void *dev, uint32_t data);

int ptdr_get_numtimes(void *dev, uint32_t *data);

int ptdr_set_durations(void *dev, uint32_t data);

int ptdr_get_durations(void *dev, uint32_t *data);

int ptdr_set_route(void *dev, uint32_t data);

int ptdr_get_route(void *dev, uint32_t *data);

int ptdr_set_position(void *dev, uint32_t data);

int ptdr_get_position(void *dev, uint32_t *data);

int ptdr_set_departure(void *dev, uint32_t data);

int ptdr_get_departure(void *dev, uint32_t *data);

int ptdr_set_seed(void *dev, uint32_t data);

int ptdr_get_seed(void *dev, uint32_t *data);

int ptdr_set_base(void *dev, uint64_t data);

int ptdr_get_base(void *dev, uint64_t *data);

int ptdr_interruptglobal(void *dev, int enable);

int ptdr_set_interruptconf(void *dev, uint32_t data);

int ptdr_get_interruptconf(void *dev, uint32_t *data);

int ptdr_get_interruptstatus(void *dev, uint32_t *data);

int ptdr_route_parse(char *buff, size_t buff_size, ptdr_route_t *route, size_t *route_size);

#ifdef DEBUG
int ptdr_reg_dump(void *dev);
int ptdr_ctrl_dump(void *dev);
#endif

#endif
