VERSION=v1.1
CRYPTO=OPENSSL
DEF_=-DNO_CRYPTO
CRYPTO_DEF=$(DEF_$(CRYPTO))

TOP_DIR := $(shell pwd)
LOCAL_PATH := $(shell pwd)
APP = publish

CC = g++
DEF=-DRTMPDUMP_VERSION=\"$(VERSION)\" $(CRYPTO_DEF) $(XDEF)
CFLAGS = -g -Wall $(DEF)#-fPIC 
CFLAGS += -I$(TOP_DIR)/include
LIBS = -lpthread -lm -lrtmp  -lavcodec-57 -lavdevice-57 -lavfilter-6 -lavformat-57 -lavutil-55 -lswscale-4 -lswresample-2
LIBS +=-lopencv_objdetect  -lopencv_highgui -lopencv_imgproc -lopencv_core -lopencv_imgcodecs  -lopencv_video -lopencv_videoio -lopencv_videostab 
DEP_LIBS = -L$(TOP_DIR)/lib
LD_LIBRARY_PATH = -Wl,-rpath,$(TOP_DIR)/lib
HEADER =
OBJS =  thread.o server.o rtmpserver.o  send264.o  control.o message.o camera.o 

all:  $(OBJS)
	$(CC) $(CFLAGS) -o $(APP) $(OBJS) $(DEP_LIBS) $(LIBS) $(LD_LIBRARY_PATH)

.cpp.o:
	$(CC) $(CFLAGS) -o $@ -c $<
	
#rtmpserver.o: rtmpserver.cpp  server.h
#thread.o: thread.c thread.h

clean:
	rm -f *.o a.out $(APP) core *~







