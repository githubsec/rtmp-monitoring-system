/*
 * cameraupdate.cpp
 *
 *  Created on: 2017年2月17日
 *      Author: c
 */
#include "include/camera.h"

FILE *infile=fopen("infile.yuv","r");
FILE *outfile=fopen("outfile.yuv","wa+");

void errno_exit(const char *s)
{
	std::cout<<*s<<std::endl;
}

void open_camera(Camera* cam)
{
	cam->fd=open(cam->device_name,O_RDWR);
	if(cam->fd==-1)
	{
			cout<<"Cannot open the device."<<endl;
			exit(1);
	}
	else
	{
		//cout<<"Open the device."<<endl;
	}
}

void close_camera(Camera* cam){
	close(cam->fd);
}

/*void encode_frame(Camera* cam,unsigned int length,int i)
{
	if(i==200)
	{
		rewind(infile);
		//fseek(infile,0,SEEK_SET);
	}
	unsigned char *yuv_frame=(unsigned char*)malloc(length);//=static_cast<unsigned char *>(cam->buffers[i].start);
	//fseek(infile,i*length,SEEK_SET);
	fread(yuv_frame,length,1,infile);
	if(yuv_frame[0]=='\0')
	{
		cout<<"yuv_frame[0]=='\0' "<<endl;
				return;
	}

	mBuffer *inBuffer=(mBuffer*)malloc(sizeof(mBuffer));
	inBuffer->mpBuffer=(char*)malloc(length);
	memcpy(inBuffer->mpBuffer, yuv_frame, length);
	//inBuffer->mpBuffer=(char*)yuv_frame;
	inBuffer->mSize=length;
	putBufferWithData(&cam->buffer_list,inBuffer );

	unsigned char *y420p_buffer=(unsigned char*)malloc(length/2*1.5);
	convert_yuyv_to_yuv420p(yuv_frame,y420p_buffer,cam->width,cam->height);
	encode(cam,y420p_buffer);


	free(y420p_buffer);
	free(yuv_frame);
	//fwrite(inBuffer->mpBuffer,length,1,outfile);
	cout<<"fwrite done."<<endl;
}*/

void save_frame(Camera* cam,unsigned int length)
{
	TIMER starttime,endtime;
	TIMER sumstart,sumend;
	int encodetime,onetime;

	GETTIME(&sumstart);
	if(cam->buffer_list.buffer_count==0)
	{
		cout<<"List is empty."<<endl;
		return;
	}
	unsigned char *yuv_frame=(unsigned char*)malloc(length);
	pthread_mutex_lock(&cam->buffer_list.mutex);
	mBuffer  *pBufferEntry = list_first_entry(&cam->buffer_list.inReadyBufferList, mBuffer, mList);
	memcpy(yuv_frame, pBufferEntry->mpBuffer, length);
	list_move_tail(&pBufferEntry->mList, &cam->buffer_list.inBufferList);
	cam->buffer_list.buffer_count--;
	pthread_mutex_unlock(&cam->buffer_list.mutex);
	//endcode
	unsigned char *y420p_buffer=(unsigned char*)malloc(length*1.5);
	//convert_yuyv_to_yuv420p(yuv_frame,y420p_buffer,cam->width,cam->height);
	GETTIME(&starttime);
//	encode(cam,y420p_buffer);
	GETTIME(&endtime);
	ELAPSEDTIME(starttime,endtime, encodetime, frequency);
	fwrite(yuv_frame,length,1,outfile);
	GETTIME(&sumend);
	ELAPSEDTIME(sumstart,sumend, onetime, frequency);
//	cout<<"sumtime:"<<onetime<<endl;
//	cout<<"encode time:"<<encodetime<<endl;

	free(y420p_buffer);
	free(yuv_frame);
}
Ptr<BackgroundSubtractorMOG2> mog;
Ptr<BackgroundSubtractorMOG2> mogchange;
Mat foreground;
Mat frame;
Mat result;
Mat se;
Mat bw;


Mat backP;
Mat mask;
vector<vector<Point> > contours;
Rect rt;

Rect trackWindow;
RotatedRect trackBox;//定义一个旋转的矩阵类对象
int hsize = 16;
float hranges[] = { 0,180 };//hranges在后面的计算直方图函数中要用到
const float* phranges = hranges;
int vmin = 10, vmax = 256, smin = 30;
int trackObject = -1;
Mat hue;
Mat hist;

bool biggerSort(vector<Point> v1, vector<Point> v2)
{
	return contourArea(v1)>contourArea(v2);
}

int target(Camera* cam)
{
	Mat yuv;
	yuv.create(cam->height*3/2,cam->width,CV_8UC1);
	memcpy(yuv.data,cam->y420p_buffer,cam->height*cam->width*1.5*sizeof(unsigned char));
	cvtColor(yuv,frame,CV_YUV2BGR_I420);

	if (cam->num_bg<50)
	{
		cam->num_bg++;

		if (frame.empty() == true)
		{
			cout << "视频帧太少，无法训练背景" << endl;
			getchar();
			return 1;
		}
		mog->apply(frame,cam-> foreground, 0.01);
		if(cam->num_bg>=50)
		{
			mogchange->apply( cam-> foreground,  foreground, 1);
			se = getStructuringElement(MORPH_RECT, Size(20, 20));
		}
	}

	if (cam->flag_goal!=1&&cam->num_bg>=50)
	{
		mogchange->apply(frame, foreground, 0.01);
		medianBlur(foreground, foreground, 5);
		morphologyEx(foreground, foreground, MORPH_DILATE, se);
		foreground.copyTo(bw);
		findContours(bw, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
		if (contours.size() < 1)
			return 1;
		std::sort(contours.begin(), contours.end(), biggerSort);
		rt = boundingRect(contours[0]);
		cam->num_goal++;
		if (cam->num_goal > 100&& rt.width*rt.height >cam-> width*cam->height / 64)//&& rt.width*rt.height< width*height / 16 && contours.size()>3)
		{
			cam->flag_goal=1;
			for (int k = 0; k<contours.size(); ++k)
			{
				if (contourArea(contours[k])<contourArea(contours[0]) / 5)
					break;
				rt = boundingRect(contours[k]);
				cam->rts.push_back(rt);
			}
			return 0 ;
		}
	}

	if (cam->flag_goal == 1)
		{
		if (cam->num_run > 20)
		{
			cam->num_run= 0;
			cam->num_cont=0;
			cam->flag_goal=0;
			cam->num_goal=0;
			trackObject=-1;
			mogchange->apply(  cam-> foreground,foreground, 1);
		}
			Mat  hsv,  mask, histimg = Mat::zeros(200, 320, CV_8UC3), backproj;

			frame.copyTo(result);


			cvtColor(result, hsv, CV_BGR2HSV);

			if (1)
			{
				int _vmin = vmin, _vmax = vmax;

				//inRange函数的功能是检查输入数组每个元素大小是否在2个给定数值之间，可以有多通道,mask保存0通道的最小值，也就是h分量
				//这里利用了hsv的3个通道，比较h,0~180,s,smin~256,v,min(vmin,vmax),max(vmin,vmax)。如果3个通道都在对应的范围内，则
				//mask对应的那个点的值全为1(0xff)，否则为0(0x00).
				inRange(hsv, Scalar(0, smin, MIN(_vmin, _vmax)),
					Scalar(180, 256, MAX(_vmin, _vmax)), mask);
				int ch[] = { 0, 0 };
				hue.create(hsv.size(), hsv.depth());//hue初始化为与hsv大小深度一样的矩阵，色调的度量是用角度表示的，红绿蓝之间相差120度，反色相差180度
				mixChannels(&hsv, 1, &hue, 1, ch, 1);//将hsv第一个通道(也就是色调)的数复制到hue中，0索引数组

				if (trackObject < 0)
				{
					//此处的构造函数roi用的是Mat hue的矩阵头，且roi的数据指针指向hue，即共用相同的数据，select为其感兴趣的区域
					Mat roi(hue, rt), maskroi(mask, rt);//mask保存的hsv的最小值

																		//calcHist()函数第一个参数为输入矩阵序列，第2个参数表示输入的矩阵数目，第3个参数表示将被计算直方图维数通道的列表，第4个参数表示可选的掩码函数
																		//第5个参数表示输出直方图，第6个参数表示直方图的维数，第7个参数为每一维直方图数组的大小，第8个参数为每一维直方图bin的边界
					calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);//将roi的0通道计算直方图并通过mask放入hist中，hsize为每一维直方图的大小
					normalize(hist, hist, 0, 255, CV_MINMAX);//将hist矩阵进行数组范围归一化，都归一化到0~255

					trackWindow = rt;
					trackObject = 1;


				}

				calcBackProject(&hue, 1, 0, hist, backproj, &phranges);//计算直方图的反向投影，计算hue图像0通道直方图hist的反向投影，并让入backproj中
				backproj &= mask;


				RotatedRect trackBox = CamShift(backproj, trackWindow,               //trackWindow目标区域，TermCriteria为确定迭代终止的准则
					TermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1));//CV_TERMCRIT_EPS是通过forest_accuracy,CV_TERMCRIT_ITER

				 cam->num_cont++;
				if (trackWindow.area()>cam->height*cam->width*2/3||
						trackWindow.height>cam->height*4/5||trackWindow.area() <= 100||
						trackWindow.width>cam->width*4/5)
				{
					cam->num_run++;
						if (cam->num_cont - cam->num_run > 10)
					{
							cam->num_run = 0;
							cam->num_cont=0;
					}
				 }
				if (trackWindow.area() <= 1)                                                  //是通过max_num_of_trees_in_the_forest
				{
					int cols = backproj.cols, rows = backproj.rows, r = (MIN(cols, rows) + 5) / 6;
					trackWindow = Rect(trackWindow.x - r, trackWindow.y - r,
						trackWindow.x + r, trackWindow.y + r) &
						Rect(0, 0, cols, rows);//Rect函数为矩阵的偏移和大小，即第一二个参数为矩阵的左上角点坐标，第三四个参数为矩阵的宽和高
				}
				rectangle(result, trackWindow, Scalar(0, 0, 255), 5);
			}
			cvtColor(result,yuv,CV_BGR2YUV_I420);
			memcpy(cam->y420p_buffer,yuv.data,cam->height*cam->width*1.5);
			//imshow("Camshift", result);
			//moveWindow("Camshift", 0, 0);
		//	waitKey(30);

		}
	return 0;
}

void encode_frame(Camera* cam,unsigned int length,int i)
{
	unsigned char *yuv_frame=static_cast<unsigned char *>(cam->buffers[i].start);
	if(yuv_frame[0]=='\0')
	{
		cout<<"yuv_frame[0]=='\0' "<<endl;
				return;
	}
	//fwrite(yuv_frame,length,1,outfile);

	/*mBuffer *inBuffer=(mBuffer*)malloc(sizeof(mBuffer));//还需判断是否申请成功没有
	inBuffer->mpBuffer=(char*)malloc(length);
	memcpy(inBuffer->mpBuffer, yuv_frame, length);
	//inBuffer->mpBuffer=(char*)yuv_frame;
	inBuffer->mSize=length;
	putBufferWithData(&cam->buffer_list,inBuffer );*/

	cam->y420p_buffer=(unsigned char*)malloc(length/2*1.5);
	convert_yuyv_to_yuv420p(yuv_frame,cam->y420p_buffer,cam->width,cam->height);

	target(cam);
	//encode(cam,cam->y420p_buffer);
	//free(yuv_frame);
	//cout<<"fwrite done."<<endl;
}
int read_and_encode_frame(Camera* cam)
{
	struct v4l2_buffer capture_buf;
	memset(&capture_buf, 0, sizeof(capture_buf));
	capture_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	capture_buf.memory = V4L2_MEMORY_MMAP;
	/* 灏嗗凡缁忔崟鑾峰ソ瑙嗛鐨勫唴瀛樻媺鍑哄凡鎹曡幏瑙嗛鐨勯槦鍒� */
	if (ioctl(cam->fd, VIDIOC_DQBUF, &capture_buf) < 0)
	{
		cout<<"cannot get buf"<<endl;
	}

	//cout<<"read_and_encode_frame"<<endl;
	encode_frame(cam,capture_buf.length,capture_buf.index);

	if (-1 == ioctl(cam->fd, VIDIOC_QBUF, &capture_buf))
				return -1;

//image_data_handle(buffer[capture_buf.index].start, capture_buf.bytesused);

	return 0;
}

 void start_capturing(Camera* cam)
 {
	 struct v4l2_buffer buf;
	 enum v4l2_buf_type type;
	 for (int i = 0; i < 4; i++)
	 {
		 memset(&buf, 0, sizeof(buf));
		 buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		 buf.memory = V4L2_MEMORY_MMAP;
		 buf.index = i;
//		 buf.m.offset = buffer[i].offset;
		 // 灏嗙┖闂茬殑鍐呭瓨鍔犲叆鍙崟鑾疯棰戠殑闃熷垪
		 if (ioctl(cam->fd, VIDIOC_QBUF, &buf) < 0)
		 {

		 }
	 }

	 type =  V4L2_BUF_TYPE_VIDEO_CAPTURE;;
	 // 鎵撳紑璁惧瑙嗛娴� /
	 if (ioctl(cam->fd, VIDIOC_STREAMON, &type) < 0)
	 {

	 }
	 //cout<<"STREAMON"<<endl;
}

void stop_capturing(Camera* cam)
{
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == ioctl(cam->fd, VIDIOC_STREAMOFF, &type))
		errno_exit("VIDIOC_STREAMOFF");

}

void init_camera(Camera* cam){
	struct v4l2_capability cap;

	if (-1 == ioctl(cam->fd, VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			fprintf(stderr, "%s is no V4L2 device\n", cam->device_name);
			exit(EXIT_FAILURE);
		}
		else
		{
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		fprintf(stderr, "%s is no video capture device\n", cam->device_name);
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING))
	{
		fprintf(stderr, "%s does not support streaming i/o\n",cam->device_name);
		exit(EXIT_FAILURE);
	}

	//#ifdef DEBUG_CAM
	//printf("\nVIDOOC_QUERYCAP\n");
	////printf("the camera driver is %s\n", cap.driver);
	//printf("the camera card is %s\n", cap.card);
	//printf("the camera bus info is %s\n", cap.bus_info);
	//printf("the version is %d\n", cap.version);


	struct v4l2_format fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = cam->width;
	fmt.fmt.pix.height = cam->height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if (ioctl(cam->fd, VIDIOC_S_FMT, &fmt) < 0)
	{
		close(cam->fd);
	}


	//cout<<"init the camera."<<endl;

	mog = createBackgroundSubtractorMOG2();
	mogchange = createBackgroundSubtractorMOG2();
	mog->setVarThreshold(36);
	mog->setNMixtures(1);
	mogchange->setVarThreshold(36);
	mogchange->setNMixtures(5);

	init_mmap(cam);
}

void uninit_camera(Camera* cam)
{
	unsigned int i;

	for (i = 0; i < 4; ++i)
	{
		if (-1 == munmap(cam->buffers[i].start,cam->buffers[i].length))
		{
			errno_exit("munmap");
			return;
		}
	}
	free(cam->buffers);
}
void init_mmap(Camera* cam)
{
	struct v4l2_requestbuffers req;/* 鐢宠璁惧鐨勭紦瀛樺尯 */
	memset(&req, 0, sizeof(req));
	req.count = 4;  //鐢宠涓�涓嫢鏈夊洓涓紦鍐插抚鐨勭紦鍐插尯
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(cam->fd, VIDIOC_REQBUFS, &req) < 0)
	{
		fprintf(stderr, "Request buffers failure.\n");
		exit(EXIT_FAILURE);
	}
	if (req.count < 2)
	{
		fprintf(stderr, "Insufficient buffer memory on %s\n",
				cam->device_name);
		return;
	}
	cam->buffers = (Buffer *)calloc(req.count, sizeof(*cam->buffers));
	struct v4l2_buffer buf;
	for (unsigned int numBufs = 0; numBufs < req.count; numBufs++)
	{//鏄犲皠鎵�鏈夌殑缂撳瓨
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = numBufs;
		if (ioctl(cam->fd, VIDIOC_QUERYBUF, &buf) == -1)
		{//鑾峰彇鍒板搴攊ndex鐨勭紦瀛樹俊鎭紝姝ゅ涓昏鍒╃敤length淇℃伅鍙妎ffset淇℃伅鏉ュ畬鎴愬悗闈㈢殑mmap鎿嶄綔銆�
			return ;
		}
		cam->buffers[numBufs].length = buf.length;
		// 杞崲鎴愮浉瀵瑰湴鍧�
		cam->buffers[numBufs].start = mmap(NULL, buf.length,PROT_READ | PROT_WRITE,
			MAP_SHARED,
			cam->fd, buf.m.offset);
		if (cam->buffers[numBufs].start == MAP_FAILED)
		{
			return ;
		}
	}
	//cout<<"mmap the camera."<<endl;
}

void init_file(Camera *cam)
{
	cam->yuv_fp=fopen(cam->yuv_filename,"wa+");
}

void close_file(Camera *cam)
{
	fclose(cam->yuv_fp);
}

int convert_yuyv_to_yuv420p(unsigned char *yuyv, unsigned char *yuv420p, unsigned int width, unsigned int height)
{
	unsigned int in;
	unsigned int pixel_16;
	int outy=0;
	int outu=width*height;
	int outv=width*height*5/4;
	int i=0;
	for(in = 0; in < width * height * 2; in += 4)
	{
		yuv420p[outy++] = yuyv[in + 0];
		yuv420p[outy++] = yuyv[in + 2];
		if(i<width*2)
		{
			yuv420p[outu++] =yuyv[in + 1];
			yuv420p[outv++] =yuyv[in + 3];
		}
		else
		{
			;
		}
		i+=4;
		if(i>=2*2*width) i=0;
	}


	/*int widthStep422=2*width;
	for(i = 0; i < height; i += 2)
	{
		p422 = in + i * widthStep422;

		for(j = 0; j < widthStep422; j+=4)
		{
			*(y++) = p422[j];
			*(u++) = p422[j+1];
			*(y++) = p422[j+2];
		}

		p422 += widthStep422;

		for(j = 0; j < widthStep422; j+=4)
		{
			*(y++) = p422[j];
			*(v++) = p422[j+3];
			*(y++) = p422[j+2];
		}
	}*/

	//FILE *fp=fopen("oneframe.yuv","wa+");
	//fwrite(yuv420p,width*height*1.5,1,fp);
	//fclose(fp);
	return 0;
}

int encode_init(Camera*cam)
{
	cam->encode.pCodecCtx= NULL;
	cam->encode.framecnt=0;

	int ret;
	AVCodecID codec_id=AV_CODEC_ID_H264;
	int in_w=cam->width,in_h=cam->height;

	avcodec_register_all();
	cam->encode.pCodec = avcodec_find_encoder(codec_id);
	if (!cam->encode.pCodec)
	{
		printf("Codec not found\n");
		return -1;
	}
	cam->encode.pCodecCtx = avcodec_alloc_context3(cam->encode.pCodec);
	if (!cam->encode.pCodecCtx)
	{
		printf("Could not allocate video codec context\n");
		return -1;
	}
	cam->encode.pCodecCtx->bit_rate = 900000;
	cam->encode.pCodecCtx->width = in_w;
	cam->encode.pCodecCtx->height = in_h;
	cam->encode.pCodecCtx->time_base.num=1;
	cam->encode.pCodecCtx->time_base.den=25;
	cam->encode.pCodecCtx->gop_size = 250;
	cam->encode.pCodecCtx->max_b_frames = 0;
	cam->encode.pCodecCtx->qmin=10;
	cam->encode.pCodecCtx->qmax=51;
	cam->encode.pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	cam->encode.pCodecCtx->thread_count=1;
	//cam->encode.pCodecCtx->slice_count=0;


	AVDictionary *param=0;
	if(cam->encode.pCodecCtx->codec_id == AV_CODEC_ID_H264) {
	        av_dict_set(&param, "preset", "fast", 0);
	        av_dict_set(&param, "tune", "zerolatency", 0);
	        //av_dict_set(¶m, "profile", "main", 0);
	    }
	/*if (codec_id == AV_CODEC_ID_H264)
	{
		av_opt_set(cam->encode.pCodecCtx->priv_data, "preset", "ultrafast", 0);
		av_opt_set(cam->encode.pCodecCtx->priv_data, "tune", "zerolatency", 0);
		av_opt_set(cam->encode.pCodecCtx->priv_data, "profile", "high", 0);
		av_opt_set(cam->encode.pCodecCtx->priv_data, "level", "5.1", 0);
	}*/

	if (avcodec_open2(cam->encode.pCodecCtx, cam->encode.pCodec, &param) < 0)
	{
		printf("Could not open codec\n");
		return -1;
	}

	cam->encode.pFrame = av_frame_alloc();
	if (!cam->encode.pFrame)
	{
		printf("Could not allocate video frame\n");
		return -1;
	}
	cam->encode.pFrame->format = cam->encode.pCodecCtx->pix_fmt;
	cam->encode.pFrame->width  = cam->encode.pCodecCtx->width;
	cam->encode.pFrame->height = cam->encode.pCodecCtx->height;

	ret = av_image_alloc(cam->encode.pFrame->data, cam->encode.pFrame->linesize, cam->encode.pCodecCtx->width, cam->encode.pCodecCtx->height,
			cam->encode.pCodecCtx->pix_fmt, 16);
	if (ret < 0)
	{
		printf("Could not allocate raw picture buffer\n");
		return -1;
	}
	av_init_packet(&cam->encode.pkt);

	cam->encode.hevc_out = fopen(cam->hevc_filename, "wb");
	if (!cam->encode.hevc_out)
	{
		printf("Could not open %s\n", cam->encode.hevc_out);
		return -1;
	}

	return 0;
}

int encode(Camera*cam)
{
	int got_output,ret;
	unsigned char *yuv420p;
	int y_size;

	//Encode
	cam->encode.pkt.data = NULL;    // packet data will be allocated by the encoder
	cam->encode.pkt.size = 0;

	//Read raw YUV data
	read_and_encode_frame(cam);
	y_size = cam->encode.pCodecCtx->width * cam->encode.pCodecCtx->height;
	yuv420p=cam->y420p_buffer;
	cam->encode.pFrame->data[0]=yuv420p;
	cam->encode.pFrame->data[1]=yuv420p+y_size;
	cam->encode.pFrame->data[2]=yuv420p+5*y_size/4;
	cam->encode.pFrame->pts = cam->encode.framecnt;

	/* encode the image */
	ret = avcodec_encode_video2(cam->encode.pCodecCtx, &cam->encode.pkt, cam->encode.pFrame, &got_output);
	free(cam->y420p_buffer);
	if (ret < 0)
	{
		printf("Error encoding frame\n");
		return -1;
	}
	if (got_output)
	{
		//printf("Succeed to encode frame: %5d\tsize:%5d\n",cam->encode.framecnt,cam->encode.pkt.size);
		//cam->encode.framecnt++;
		//Change this code to remember file's current address.
		//fwrite(cam->encode.pkt.data, 1, cam->encode.pkt.size, cam->encode.hevc_out);
		//av_free_packet(&cam->encode.pkt);
		return 0;
	}
	return 0;
}

//have questions
void encode_uninit(Camera*cam)
{
	fclose(cam->encode.hevc_out);
	avcodec_close(cam->encode.pCodecCtx);
	av_free(cam->encode.pCodecCtx);
	av_freep(&cam->encode.pFrame->data[0]);
	av_frame_free(&cam->encode.pFrame);
}






