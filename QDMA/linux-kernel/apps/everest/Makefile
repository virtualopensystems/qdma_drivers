#/*
# * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
# * Reproduction and communication of this document is strictly prohibited
# * unless specifically authorized in writing by Virtual Open Systems.
# * ****************************************************************************
# * File Name   : Makefile
# * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
# * Description :
# *
# */
CC ?= gcc

CFLAGS += -g
#CFLAGS += -O2 -fno-inline -Wall -Wstrict-prototypes
CFLAGS += -I. -I../include -I../dma-utils -I./qdma
CFLAGS += $(EXTRA_FLAGS)
ifeq ($(DEBUG),1)
  CFLAGS += -g -DDEBUG -Wall
endif
ifeq ($(DEBUG_DEV),1)
  CFLAGS += -DDEBUG_DEV
endif
ifeq ($(DEBUG_QDMA),1)
  CFLAGS += -DDEBUG_QDMA
endif
ifeq ($(HBM16GB),1)
  CFLAGS += -DHBM16GB
endif

DMA-UTILS_OBJS := $(patsubst %.c,%.o,$(wildcard ../dma-utils/*.c))
QDMA_OBJS := $(patsubst %.c,%.o,$(wildcard ./qdma/*.c))

HELM-TEST = helm-test
HELM-TEST_OBJS += $(DMA-UTILS_OBJS)
HELM-TEST_OBJS += $(QDMA_OBJS)
HELM-TEST_OBJS += $(patsubst %.c,%.o,$(wildcard ./helm/*.c))

PTDR-TEST = ptdr-test
PTDR-TEST_OBJS += $(DMA-UTILS_OBJS)
PTDR-TEST_OBJS += $(QDMA_OBJS)
PTDR-TEST_OBJS += $(patsubst %.c,%.o,$(wildcard ./ptdr/*.c))

ifneq ($(CROSS_COMPILE_FLAG),)
	CC=$(CROSS_COMPILE_FLAG)gcc
endif

.PHONY: all
all: clean helm-test ptdr-test

.PHONY: helm-test
helm-test: $(HELM-TEST_OBJS)
	$(CC) -pthread -lrt  $^ -o $(HELM-TEST) -laio -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_LARGE_FILE_SOURCE

.PHONY: ptdr-test
ptdr-test: $(PTDR-TEST_OBJS)
	$(CC) -pthread -lrt  $^ -o $(PTDR-TEST) -laio -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_LARGE_FILE_SOURCE

%.o: %.c
	$(CC) $(CFLAGS) -c -std=c99 -o $@ $< -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_LARGE_FILE_SOURCE -D_AIO_AIX_SOURCE

.PHONY: clean
clean:
	@rm -f *.o */*.o ../dma-utils/*.o
	rm -rf *.o *.bin $(HELM-TEST) $(PTDR-TEST)

