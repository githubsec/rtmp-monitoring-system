LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := polarssl
LOCAL_EXPORT_C_INCLUDES := $(SSL)/include
LOCAL_SRC_FILES := $(SSL)/lib/libpolarssl.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(NDK_PROJECT_PATH)/librtmp \
    $(SSL)/include

LOCAL_SRC_FILES:= \
    amf.c \
   hashswf.c \
   log.c \
   parseurl.c \
   rtmp.c

LOCAL_STATIC_LIBRARIES = polarssl
LOCAL_CFLAGS += -I$(SSL)/include -DUSE_POLARSSL
LOCAL_LDLIBS += -L$(SSL)/lib -L$(SSL)/usr/lib
LOCAL_LDLIBS += -lz

LOCAL_MODULE := librtmp

include $(BUILD_SHARED_LIBRARY)