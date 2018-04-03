// ffmpegdemo.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <stdio.h>
#define __STDC_CONSTANT_MACROS
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL2/SDL.h"
};
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit=0;
int sdlfunc(void *p)
{
	thread_exit=0;
	while(!thread_exit)
	{
		SDL_Event sdlevent;
		sdlevent.type=SFM_REFRESH_EVENT;
		SDL_PushEvent(&sdlevent);
		SDL_Delay(40);
	}
	thread_exit=0;
	SDL_Event sdlevent;
	sdlevent.type=SFM_BREAK_EVENT;
	SDL_PushEvent(&sdlevent);
	return 0;
}
int _tmain(int argc, _TCHAR* argv[])
{
	AVFormatContext *pAVFormatCtx;
	int videoindex=-1,i=0;
	AVCodecContext *pAVCodecCtx;
	AVCodec *pAVCodec;
	AVFrame *pAVFrame,*pAVFrameYUV;
	uint8_t *out_buffer;
	
	AVPacket *pAVPacket;

	int ret,got_pictureFlag=0;
	int screen_w,screen_h,dbgcnt=0;
	SDL_Window *sdlcreen;
	SDL_Renderer *sdlrender;
	SDL_Texture *sdltexture;
	SDL_Rect sdlrect;
	SDL_Thread *sdlthread;
	SDL_Event sdlevent;

	struct SwsContext *pswsCtx;
	char filepath[]="屌丝男士.mov";
	
	av_register_all();
	avformat_network_init();
	pAVFormatCtx=avformat_alloc_context();
	ret=avformat_open_input(&pAVFormatCtx,filepath,NULL,NULL);
	if(ret)
	{
		printf("avformat_open_input error\n");
		goto END;
	}
	ret=avformat_find_stream_info(pAVFormatCtx,NULL);
	if(ret<0)
	{
		printf("avformat_find_stream_info error\n");
		goto END;
	}
	for(i=0;i<pAVFormatCtx->nb_streams;i++)
	{
		if(pAVFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
		{
			videoindex=i;
			break;
		}
	}
	if(videoindex==-1)
	{
		printf("Didn't find a video stream.\n");
		goto END;
	}
	pAVCodecCtx=pAVFormatCtx->streams[videoindex]->codec;
	pAVCodec=avcodec_find_decoder(pAVCodecCtx->codec_id);
	if(pAVCodec==NULL)
	{
		printf("Didn't find a pAVCodec.\n");
		goto END;
	}
	if(avcodec_open2(pAVCodecCtx, pAVCodec,NULL)<0)
	{
		printf("Could not open codec.\n");
		return -1;
	}
	pAVFrame=av_frame_alloc();
	pAVFrameYUV=av_frame_alloc();
	out_buffer=(uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P,pAVCodecCtx->width,pAVCodecCtx->height));
	avpicture_fill((AVPicture*)pAVFrameYUV,out_buffer,PIX_FMT_YUV420P,pAVCodecCtx->width,pAVCodecCtx->height);
	pswsCtx=sws_getContext(pAVCodecCtx->width,pAVCodecCtx->height,pAVCodecCtx->pix_fmt,
		pAVCodecCtx->width, pAVCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
	if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER))
	{
		printf("SDL_Init error\n");
		goto END;
	}
	screen_w=pAVCodecCtx->width;
	screen_h=pAVCodecCtx->height;
	sdlcreen=SDL_CreateWindow("test",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,screen_w,screen_h,SDL_WINDOW_OPENGL);
	if(!sdlcreen)
	{
		printf("SDL_CreateWindow error\n");
		goto END;
	}
	sdlrender=SDL_CreateRenderer(sdlcreen,-1,0);
	sdltexture=SDL_CreateTexture(sdlrender,SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING,screen_w,screen_h);
	sdlrect.x=0;
	sdlrect.y=0;
	sdlrect.w=screen_w;
	sdlrect.h=screen_h;
	pAVPacket=(AVPacket*)av_malloc(sizeof(AVPacket));//这里是AVPacket结构体的大小，而不是pAVPacket指针大小，否则会有类似段错误这样的问题，没法解码
	sdlthread=SDL_CreateThread(sdlfunc,NULL,NULL);
	while(1)
	{
		SDL_WaitEvent(&sdlevent);
		if(sdlevent.type==SFM_REFRESH_EVENT)
		{
			if(av_read_frame(pAVFormatCtx,pAVPacket)>=0)
			{
				if(pAVPacket->stream_index==videoindex)
				{
					ret=avcodec_decode_video2(pAVCodecCtx,pAVFrame,&got_pictureFlag,pAVPacket);//如果前面没有avcodec_open2打开解码器，那么pAVCodecCtx->pAVCodec会为0，引起无效参数错误
					printf("%p,%p,%p,%p\n",pAVCodecCtx,pAVFrame,&got_pictureFlag,pAVPacket);
					if(ret < 0)
					{
						printf("Decode Error.\n");
						goto END;
					}
					printf("decode %d\n",dbgcnt++);
					if(got_pictureFlag)
					{
						sws_scale(pswsCtx,(const uint8_t* const*)pAVFrame->data,
							pAVFrame->linesize,0, pAVCodecCtx->height, pAVFrameYUV->data, pAVFrameYUV->linesize);

						SDL_UpdateTexture(sdltexture,NULL,pAVFrameYUV->data[0], pAVFrameYUV->linesize[0]);
						SDL_RenderClear( sdlrender );
						SDL_RenderCopy(sdlrender,sdltexture,NULL,NULL);
						SDL_RenderPresent(sdlrender);
					}
				}
				av_free_packet(pAVPacket);
			}
			else//无视频流，退出程序
			{
				thread_exit=1;
			}
		}
		else if(SDL_QUIT==sdlevent.type)
		{
			thread_exit=1;
		}
		else if(SFM_BREAK_EVENT==sdlevent.type)
		{
			break;
		}
	}
	sws_freeContext(pswsCtx);
	SDL_Quit();
	av_frame_free(&pAVFrameYUV);
	av_frame_free(&pAVFrame);
	avcodec_close(pAVCodecCtx);
	avformat_close_input(&pAVFormatCtx);

	END:
	system("pause");
	return 0;
}

