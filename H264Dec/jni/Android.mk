# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Qualcomm Note:
#     Modify this makefile to only build the shared libraries based
#     on the Android OS shared libraries you have pre-built in
#     the Android source.
#

LOCAL_PATH:= $(call my-dir)


# Common libraries for jb

# FFmpeg library

include $(CLEAR_VARS)
LOCAL_MODULE := avcodec
LOCAL_SRC_FILES := ffmpeg/libavcodec-56.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := avdevice
LOCAL_SRC_FILES := ffmpeg/libavdevice-56.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := avfilter
LOCAL_SRC_FILES := ffmpeg/libavfilter-5.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := avformat
LOCAL_SRC_FILES := ffmpeg/libavformat-56.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := avutil
LOCAL_SRC_FILES := ffmpeg/libavutil-54.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := postproc
LOCAL_SRC_FILES := ffmpeg/libpostproc-53.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := swresample
LOCAL_SRC_FILES := ffmpeg/libswresample-1.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := swscale
LOCAL_SRC_FILES := ffmpeg/libswscale-3.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := rtmp
LOCAL_SRC_FILES := librtmp/librtmp.so
include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE                    := ndk_h264dec
LOCAL_SHARED_LIBRARIES  := avcodec avdevice avfilter avformat avutil postproc swresample swscale
LOCAL_SHARED_LIBRARIES +=rtmp
LOCAL_LDLIBS                    := -llog -lGLESv2
LOCAL_SRC_FILES                 := ndkH264Decode.cpp shaders.c opengles_display.c #test.cpp
LOCAL_C_INCLUDES 				:= $(LOCAL_PATH)/ffmpeg/include 
                                   #$(LOCAL_PATH)/ffmpeg/android/full-eng/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/librtmp/include
LOCAL_CPPFLAGS                  += -fexceptions
include $(BUILD_SHARED_LIBRARY)
