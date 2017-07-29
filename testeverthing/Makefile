TOP_DIR := $(shell pwd)
LOCAL_PATH := $(shell pwd)
APP = test

CC = g++
CFLAGS = -g -Wall #-fPIC
CFLAGS += -I$(TOP_DIR)/include
LIBS = -lpthread -lm -lrtmp -lavcodec-57 -lavdevice-57 -lavfilter-6 -lavformat-57 -lavutil-55 -lswscale-4 -lswresample-2 -lSDL2 -lSDL2main
DEP_LIBS = -L$(TOP_DIR)/lib
LD_LIBRARY_PATH = -Wl,-rpath,$(TOP_DIR)/lib
HEADER =
OBJS =test.o sdl.o #simplest_ffmpeg_player_su.o 

all:  $(OBJS)
	$(CC) $(CFLAGS) -o $(APP) $(OBJS) $(DEP_LIBS) $(LIBS) $(LD_LIBRARY_PATH)

.cpp.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o a.out $(APP) core *~
