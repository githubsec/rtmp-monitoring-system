# rtmp-monitoring-system
H264Dec is an Android player client.   
includetarget is the RTMP server and encoder.   
testeverthing is an Linux player client.   

源码说明
注明：此源码运行与ubuntu 15.04 32位版本的pc机上，初次运行可能需要先行安装基本的运行库。    

一、源码结构   
分为三个部分：       
1.	根文件夹    
根文件夹中的c和cpp文件为自己编写，其中rtmpserver.cpp中包含main函数，负责程序的启动，server包含一些main函数需要使用到的函数的实现；camera.cpp文件包含数据的初始化，v4l2接口的实现，h264编码的实现，目标跟踪的实现；contorl函数负责调用camera文件中的函数，实现各项功能message函数是一套响应操作，工程中没有使用；newcamera.cpp以前的测试文件，未使用；send264.cpp负责将h264文件封装为flv格式，通过rtmp协议进行发送；test.cpp调试时使用thread.c是Linux下多线程需要使用。    
2.	Lib文件夹     
Lib文件夹中包含几大开源库编译为支持linux-x86架构的动态库，不同的平台需要重新编译。    
其中包括ffmpeg开源库、librtmp开源库和opencv开源库的动态库。    
3.	include文件夹   
include文件夹包含了工程的头文件，其中，camera.h、control.h、rtmp.h、send264.h、server.h为自己编写的主要头文件。   

二、运行流程及函数调用图   
其中开始为rtmpserver.cpp中的main函数，startServer()函数到play实现在server.cpp中，实现rtmp服务器的功能；RTMP264_send()以及ReadOneNaluFromBuf()、SendH264Packet()函数在send264.cpp中，起着将h264数据封装为flv数据并发送给客户端的功能；encode函数的实现在camera.cpp中，起着编码yuv数据为h264数据的功能；read_and_encode_frame()函数在camera.cpp中实现，从v4l2 api中获取yuv原始数据。   
   
三、主要代码解析   
Rtmpserver.cpp     
程序从这个文件中运行，包含有main函数，main函数首先初始化存储h264 nalu单元的共享内存，设置一些rtmp通信相关的设置，调用initcamera函数初始化视频采集部分等，调用encode_init初始化编码部分。    
Server.cpp     
包含rtmp通信所需要的功能的实现部分。对照3轻量级RTMP多媒体视频服务器的设计与实现阅读源码。      
Camera.cpp     
包含v4l2的初始化的函数实现过程，包含编码初始话的实现过程，包含目标跟踪的实现过程。可以对照函数名字和相关文档阅读。     
Send264.cpp    
此文件中函数的主要功能是将h264 nalu数据封装为flv格式，并使用rtmp协议发送出去。    
