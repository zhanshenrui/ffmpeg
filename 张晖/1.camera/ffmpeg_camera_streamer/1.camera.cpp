/**
 * 
 * 张晖 Hui Zhang
 * zhanghuicuc@gmail.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * 
 * 本程序实现了读取PC端摄像头数据并进行编码和流媒体传输。
 *
 */

#include <stdio.h>
#include <windows.h>
extern "C"
{
#include "libavutil\opt.h"
#include "libavcodec\avcodec.h"
#include "libavformat\avformat.h"
#include "libavutil\time.h"
#include "libavdevice\avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/mathematics.h"
};


//Show Device  
void show_dshow_device(){
	AVFormatContext *pFmtCtx = avformat_alloc_context();
	AVDictionary* options = NULL;
	av_dict_set(&options, "list_devices", "true", 0);
	AVInputFormat *iformat = av_find_input_format("dshow");
	printf("Device Info=============\n");
	avformat_open_input(&pFmtCtx, "video=dummy", iformat, &options);
	printf("========================\n");
}

int flush_encoder(AVFormatContext *ifmt_ctx,AVFormatContext *ofmt_ctx, unsigned int stream_index, int framecnt);

int exit_thread = 0;
DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
	while ((getchar()) != '\n');
	exit_thread = 1;
	return 0;
}

int main(int argc, char* argv[])
{
	AVFormatContext *ifmt_ctx=NULL;
	AVFormatContext *ofmt_ctx;
	AVInputFormat* ifmt;
	AVStream* video_st;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVPacket *dec_pkt, enc_pkt;
	AVFrame *pframe, *pFrameYUV;
	struct SwsContext *img_convert_ctx;

	char capture_name[80] = {0};
	char device_name[80] = {0};
	int framecnt=0;
	int videoindex;
	int i;
	int ret;
	HANDLE  hThread;

	const char* out_path = "rtmp://localhost/live/livestream";	 
	int dec_got_frame,enc_got_frame;

	av_register_all();
	//Register Device
	avdevice_register_all();
	avformat_network_init();
	
	//Show Dshow Device  
	show_dshow_device();
	
	printf("\nChoose capture device: ");
	if (gets(capture_name) == 0)
	{
		printf("Error in gets()\n");
		system("pause");return -1;
	}
	sprintf(device_name, "video=%s", capture_name);

	ifmt=av_find_input_format("dshow");//找到输入媒体，这里是相机
	
	//Set own video device's name
	//打开媒体并初始化
	if (avformat_open_input(&ifmt_ctx, device_name, ifmt, NULL) != 0){
		printf("Couldn't open input stream.（无法打开输入流）\n");
		system("pause");return -1;
	}
	//input initialize
	//打开媒体并初始化额外信息
	if (avformat_find_stream_info(ifmt_ctx, NULL)<0)
	{
		printf("Couldn't find stream information.（无法获取流信息）\n");
		system("pause");return -1;
	}
	videoindex = -1;
	for (i = 0; i<ifmt_ctx->nb_streams; i++)
		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoindex = i;
			break;
		}
	if (videoindex == -1)
	{
		printf("Couldn't find a video stream.（没有找到视频流）\n");
		system("pause");return -1;
	}
	//根据媒体里的流找到解码器，并打开
	if (avcodec_open2(ifmt_ctx->streams[videoindex]->codec, avcodec_find_decoder(ifmt_ctx->streams[videoindex]->codec->codec_id), NULL)<0)
	{
		printf("Could not open codec.（无法打开解码器）\n");
		system("pause");return -1;
	}

	//output initialize
	//初始化输出媒体
	avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_path);
	//output encoder initialize
	//因为是输出媒体，需要编码器，所以寻找编码器
	pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!pCodec){
		printf("Can not find encoder! (没有找到合适的编码器！)\n");
		system("pause");return -1;
	}
	pCodecCtx=avcodec_alloc_context3(pCodec);
	pCodecCtx->pix_fmt = PIX_FMT_YUV420P;
	pCodecCtx->width = ifmt_ctx->streams[videoindex]->codec->width;
	pCodecCtx->height = ifmt_ctx->streams[videoindex]->codec->height;
	pCodecCtx->time_base.num = 1;
	pCodecCtx->time_base.den = 25;
	pCodecCtx->bit_rate = 400000;
	pCodecCtx->gop_size = 250;
	/* Some formats want stream headers to be separate. */
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		pCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	//H264 codec param
	//pCodecCtx->me_range = 16;
	//pCodecCtx->max_qdiff = 4;
	//pCodecCtx->qcompress = 0.6;
	pCodecCtx->qmin = 10;//可使用的量化值，默认为10，我们也要默认的
	pCodecCtx->qmax = 51;
	//Optional Param
	pCodecCtx->max_b_frames = 3;//最大B帧数，即两个P帧之间最多3个B帧
	// Set H264 preset and tune
	AVDictionary *param = 0;
	av_dict_set(&param, "preset", "fast", 0);
	av_dict_set(&param, "tune", "zerolatency", 0);
	//初始化编解码器
	if (avcodec_open2(pCodecCtx, pCodec,&param) < 0){
		printf("Failed to open encoder! (编码器打开失败！)\n");
		system("pause");return -1;
	}

	//Add a new stream to output,should be called by the user before avformat_write_header() for muxing
	//为输出上下文分配一条流
	video_st = avformat_new_stream(ofmt_ctx, pCodec);
	if (video_st == NULL){
		system("pause");return -1;
	}
	video_st->time_base.num = 1;
	video_st->time_base.den = 25;
	video_st->codec = pCodecCtx;

	//Open output URL,set before avformat_write_header() for muxing
	//根据文件名初始化分配一个输入输出IO上下文AVIOContext
	if (avio_open(&ofmt_ctx->pb,out_path, AVIO_FLAG_READ_WRITE) < 0){
	printf("Failed to open output file! (输出文件打开失败！)\n");
	system("pause");return -1;
	}

	//Show some Information
	//打印信息
	av_dump_format(ofmt_ctx, 0, out_path, 1);

	//Write File Header
	//为输出的的上下文添加流信息头
	avformat_write_header(ofmt_ctx,NULL);

	//prepare before decode and encode
	//分配一个AVPacket，注意AVPacket用av_malloc
	dec_pkt = (AVPacket *)av_malloc(sizeof(AVPacket));
	//enc_pkt = (AVPacket *)av_malloc(sizeof(AVPacket));
	//camera data has a pix fmt of RGB,convert it to YUV420
	//申请初始化一个图像处理上下文
	img_convert_ctx = sws_getContext(ifmt_ctx->streams[videoindex]->codec->width, ifmt_ctx->streams[videoindex]->codec->height, 
		ifmt_ctx->streams[videoindex]->codec->pix_fmt, pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
	////分配一个AVFrame，注意AVFrame用av_frame_alloc
	pFrameYUV = av_frame_alloc();
	//分配一个图片内存
	uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
	//将图片内存out_buffer与pFrameYUV关联起来
	avpicture_fill((AVPicture *)pFrameYUV, out_buffer, PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
	
	printf("\n --------call started----------\n\n");
	printf("Press enter to stop...");
	hThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		MyThreadFunction,       // thread function name
		NULL,          // argument to thread function 
		0,                      // use default creation flags 
		NULL);   // returns the thread identifier 
	
	//start decode and encode
	int64_t start_time=av_gettime();
	//从输入媒体上下文中读取编码数据AVPacket,摄像头的数据是已经被摄像头编码好的数据，用AVPacket
	while (av_read_frame(ifmt_ctx, dec_pkt) >= 0){	
		if (exit_thread)
			break;
		av_log(NULL, AV_LOG_DEBUG, "Going to reencode the frame\n");
		//因为编码数据是AVPacket，解压后变成原始数据AVFrame，后面要解码，所以这里先分配一个AVFrame
		pframe = av_frame_alloc();
		if (!pframe) {
			ret = AVERROR(ENOMEM);
			system("pause");return -1;
		}
		//av_packet_rescale_ts(dec_pkt, ifmt_ctx->streams[dec_pkt->stream_index]->time_base,
		//	ifmt_ctx->streams[dec_pkt->stream_index]->codec->time_base);
		//解码，解码成功dec_got_frame会置位
		ret = avcodec_decode_video2(ifmt_ctx->streams[dec_pkt->stream_index]->codec, pframe,
			&dec_got_frame, dec_pkt);
		if (ret < 0) {
			av_frame_free(&pframe);
			av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
			break;
		}
		if (dec_got_frame){
			sws_scale(img_convert_ctx, (const uint8_t* const*)pframe->data, pframe->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);	

			enc_pkt.data = NULL;
			enc_pkt.size = 0;
			av_init_packet(&enc_pkt);
			ret = avcodec_encode_video2(pCodecCtx, &enc_pkt, pFrameYUV, &enc_got_frame);
			av_frame_free(&pframe);
			if (enc_got_frame == 1){
				//printf("Succeed to encode frame: %5d\tsize:%5d\n", framecnt, enc_pkt.size);
				framecnt++;	
				enc_pkt.stream_index = video_st->index;

				//Write PTS
				AVRational time_base = ofmt_ctx->streams[videoindex]->time_base;//调试看数据为{ 1, 1000 };
				AVRational r_framerate1 = ifmt_ctx->streams[videoindex]->r_frame_rate;// 调试看数据为{ 10000000, 400000 };/////////////
				AVRational time_base_q = { 1, AV_TIME_BASE };//宏定义为{1,1000000}////////////////////
				//Duration between 2 frames (us)
				//av_q2d()函数将结构体变为分数,time_base={ 1, 1000 };time_base_q={1,1000000}
				//y=av_rescale_q(x,bq,cq)====>y=x*bq/cq,bq是结构体要转为分数
				int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//=40000,内部时间戳  400000//////////////
				//Parameters
				//enc_pkt.pts = (double)(framecnt*calc_duration)*(double)(av_q2d(time_base_q)) / (double)(av_q2d(time_base));
				enc_pkt.pts = av_rescale_q(framecnt*calc_duration, time_base_q, time_base);
				//enc_pkt.pts=framecnt*40000*(1/1000000)/(1/1000)=79*40000*0.001=3160
				enc_pkt.dts = enc_pkt.pts;
				if(framecnt==79)
				{
					printf("\nhello,b=%d\n",enc_pkt.dts);
					//system("pause");
				}
				enc_pkt.duration = av_rescale_q(calc_duration, time_base_q, time_base); //(double)(calc_duration)*(double)(av_q2d(time_base_q)) / (double)(av_q2d(time_base));
				
				enc_pkt.pos = -1;
				
				//Delay
				if(framecnt==79)
				{
					printf("\nhello,c=%d\n",enc_pkt.dts);
					//system("pause");
				}
				int64_t pts_time = av_rescale_q(enc_pkt.dts, time_base, time_base_q);
				//pts_time=enc_pkt.dts*(1/1000)/(1/1000000)=3160000
				if(framecnt==79)
				{
					printf("\nhello,a=%d\n",enc_pkt.dts);
					//system("pause");
				}
				//printf("time_base=%d,r_framerate1");
				int64_t now_time = av_gettime() - start_time;
				if (pts_time > now_time)
					av_usleep(pts_time - now_time);
				if(framecnt==79)
				{
					printf("\nhello,d=%d\n",enc_pkt.dts);
					//system("pause");
				}
				ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
				if(framecnt==79)
				{
					printf("\nhello,e=%d\n",enc_pkt.dts);
					//system("pause");
				}
				if(framecnt==79)
				{
					printf("\nhello,x=%d\n",enc_pkt.dts);
					system("pause");
				}
				av_free_packet(&enc_pkt);
				
			}
		}
		else {
			av_frame_free(&pframe);
		}
		av_free_packet(dec_pkt);
	}
	//Flush Encoder
	//输入的像素数据读取完成后调用此函数。用于输出编码器中剩余的AVPacket
	ret = flush_encoder(ifmt_ctx,ofmt_ctx,0,framecnt);
	if (ret < 0) {
		printf("Flushing encoder failed\n");
		system("pause");return -1;
	}

	//Write file trailer
	av_write_trailer(ofmt_ctx);

	//Clean
	if (video_st)
		avcodec_close(video_st->codec);
	av_free(out_buffer);
	avio_close(ofmt_ctx->pb);
	avformat_free_context(ifmt_ctx);
	avformat_free_context(ofmt_ctx);
	CloseHandle(hThread);
	return 0;
}
//ret = flush_encoder(ifmt_ctx,ofmt_ctx,0,framecnt);
int flush_encoder(AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx, unsigned int stream_index, int framecnt){
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;
	while (1) {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_video2 (ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
			NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame){
			ret=0;
			break;
		}
		printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n",enc_pkt.size);

		//Write PTS
		AVRational time_base = ofmt_ctx->streams[stream_index]->time_base;//{ 1, 1000 };
		AVRational r_framerate1 = ifmt_ctx->streams[stream_index]->r_frame_rate;// { 50, 2 };
		AVRational time_base_q = { 1, AV_TIME_BASE };
		//Duration between 2 frames (us)
		int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//内部时间戳
		//Parameters
		enc_pkt.pts = av_rescale_q(framecnt*calc_duration, time_base_q, time_base);
		enc_pkt.dts = enc_pkt.pts;
		enc_pkt.duration = av_rescale_q(calc_duration, time_base_q, time_base);

		/* copy packet*/
		//转换PTS/DTS（Convert PTS/DTS）
		enc_pkt.pos = -1;
		framecnt++;
		ofmt_ctx->duration=enc_pkt.duration * framecnt;

		/* mux encoded frame */
		ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
}