# Copyright (C) 2014-2015, Western Digital Technologies, Inc. <copyrightagent@wdc.com>
# Platform Development Group

NAM_PROJ = smrsim
EXE_UTIL = $(NAM_PROJ)_util
DIR_KMOD = ../$(NAM_PROJ)_kmod

all: $(EXE_UTIL).c $(DIR_KMOD)/$(NAM_PROJ)_types.h  $(DIR_KMOD)/$(NAM_PROJ)_ioctl.h
	$(CC) -o $(EXE_UTIL)  $(EXE_UTIL).c -I$(DIR_KMOD) -g
clean:
	rm -f $(EXE_UTIL) *.o

