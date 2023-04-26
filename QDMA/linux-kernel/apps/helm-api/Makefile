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
CFLAGS += -I. -I../include -I../dma-utils
CFLAGS += $(EXTRA_FLAGS)
ifeq ($(DEBUG),1)
  CFLAGS += -g -DDEBUG
endif

HELM-API = helm-api
DMA-UTILS_OBJS := $(patsubst %.c,%.o,$(wildcard ../dma-utils/*.c))
HELM-API_OBJS += $(patsubst %.c,%.o,$(wildcard *.c))
HELM-API_OBJS += $(DMA-UTILS_OBJS)

ifneq ($(CROSS_COMPILE_FLAG),)
	CC=$(CROSS_COMPILE_FLAG)gcc
endif

all: clean helm-api

helm-api: $(HELM-API_OBJS)
	$(CC) -pthread -lrt  $^ -o $(HELM-API) -laio -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_LARGE_FILE_SOURCE

%.o: %.c
	$(CC) $(CFLAGS) -c -std=c99 -o $@ $< -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_LARGE_FILE_SOURCE -D_AIO_AIX_SOURCE

clean:
	@rm -f *.o */*.o ../dma-utils/*.o
	rm -rf *.o *.bin helm-api
