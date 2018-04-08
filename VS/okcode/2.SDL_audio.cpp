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
#include "libswresample/swresample.h"
#include "SDL2/SDL.h"
};
#define MAX_AUDIO_FRAME_SIZE 192000
#define OUTPUT_PCM 1
//Use SDL
#define USE_SDL 1
static  Uint8  *audio_chunk; 
static  Uint32  audio_len; 
static  Uint8  *audio_pos; 
void  fill_audio(void *udata,Uint8 *stream,int len)
{
	SDL_memset(stream, 0, len);
	if(audio_len==0)
		return;
	len=(len>audio_len?audio_len:len);
	SDL_MixAudio(stream,audio_pos,len,SDL_MIX_MAXVOLUME);
	audio_pos += len; 
	audio_len -= len; 
}
int _tmain(int argc, _TCHAR* argv[])
{
	AVFormatContext *pAVFormatCtx;
	int				i, audioStream;
	AVCodecContext *pAVCodecCtx;
	AVCodec *pAVCodec;
	AVPacket *pAVPacket;
	uint8_t *outbuffer;
	AVFrame *pAVFrame;
	SDL_AudioSpec wanted_spec;
	int iret;
	uint32_t u32len=0;
	int igotFlag=0;
	int iIndex=0;
	uint64_t in_channel_layout;
	struct SwrContext* pSwrCtx_au;
	FILE *pFile=NULL;
	char url[]="xiaoqingge.mp3";

	av_register_all();
	pAVFormatCtx = avformat_alloc_context();
	if(avformat_open_input(&pAVFormatCtx,url,NULL,NULL))
	{
		printf("Couldn't open input stream.\n");
		goto end;
	}
	if(avformat_find_stream_info(pAVFormatCtx,NULL)<0)
	{
		printf("Couldn't find stream information.\n");
		goto end;
	}
	av_dump_format(pAVFormatCtx, 0, url, false);
	audioStream=-1;
	for(i=0;i<pAVFormatCtx->nb_streams;i++)
	{
		if(pAVFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO)
		{
			audioStream=i;
			break;
		}
	}
	if(-1==audioStream)
	{
		printf("Didn't find a audio stream.\n");
		goto end;
	}
	pAVCodecCtx=pAVFormatCtx->streams[audioStream]->codec;
	pAVCodec=avcodec_find_decoder(pAVCodecCtx->codec_id);
	if(pAVCodec==NULL)
	{
		printf("Codec not found.\n");
		goto end;
	}
	if(avcodec_open2(pAVCodecCtx,pAVCodec,NULL)<0)
	{
		printf("Could not open codec.\n");
		goto end;
	}
	#if OUTPUT_PCM
	pFile=fopen("output.pcm", "wb");
	#endif
	pAVPacket=(AVPacket*)av_malloc(sizeof(AVPacket));
	//av_init_packet(packet);
	uint64_t out_channel_layout=AV_CH_LAYOUT_STEREO;
	int out_nb_samples=pAVCodecCtx->frame_size;
	AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;
	int out_sample_rate=44100;
	int out_channels=av_get_channel_layout_nb_channels(out_channel_layout);
	int out_buffer_size=av_samples_get_buffer_size(NULL,out_channels,out_nb_samples,out_sample_fmt,1);
	outbuffer=(uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE*2);
	pAVFrame=av_frame_alloc();
	#if USE_SDL
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) 
	{  
		printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
		goto end;
	}
	wanted_spec.freq = out_sample_rate;
	wanted_spec.format = AUDIO_S16SYS; 
	wanted_spec.channels = out_channels; 
	wanted_spec.silence = 0; 
	wanted_spec.samples = out_nb_samples; 
	wanted_spec.callback = fill_audio;
	wanted_spec.userdata = pAVCodecCtx;
	if (SDL_OpenAudio(&wanted_spec, NULL)<0){ 
		printf("can't open audio.\n"); 
		goto end;
	} 
	#endif
	in_channel_layout=av_get_default_channel_layout(pAVCodecCtx->channels);
	pSwrCtx_au = swr_alloc();
	pSwrCtx_au=swr_alloc_set_opts(pSwrCtx_au,out_channel_layout, out_sample_fmt, out_sample_rate,
		in_channel_layout,pAVCodecCtx->sample_fmt , pAVCodecCtx->sample_rate,0, NULL);
	swr_init(pSwrCtx_au);
	SDL_PauseAudio(0);
	while(av_read_frame(pAVFormatCtx,pAVPacket)>=0)
	{
		if(pAVPacket->stream_index==audioStream)
		{
			iret=avcodec_decode_audio4(pAVCodecCtx,pAVFrame,&igotFlag,pAVPacket);
			if ( iret < 0 ) 
			{
                printf("Error in decoding audio frame.\n");
               goto end;
            }
			if(igotFlag>0)
			{
				swr_convert(pSwrCtx_au,&outbuffer,MAX_AUDIO_FRAME_SIZE,(const uint8_t **)pAVFrame->data , pAVFrame->nb_samples);
				printf("index:%5d\t pts:%lld\t packet size:%d\n",iIndex,pAVPacket->pts,pAVPacket->size);
				#if OUTPUT_PCM
				//Write PCM
				fwrite(outbuffer, 1, out_buffer_size, pFile);
				#endif
				iIndex++;
			}
			#if USE_SDL
			while(audio_len>0)//Wait until finish
				SDL_Delay(1);
			audio_chunk = (Uint8 *) outbuffer; 
			audio_len =out_buffer_size;
			audio_pos = audio_chunk;
			#endif
		}
		av_free_packet(pAVPacket);
	}
	swr_free(&pSwrCtx_au);
	#if USE_SDL
	SDL_CloseAudio();//Close SDL
	SDL_Quit();
	#endif
	#if OUTPUT_PCM
	fclose(pFile);
	#endif
	av_free(outbuffer);
	avcodec_close(pAVCodecCtx);
	avformat_close_input(&pAVFormatCtx);
	end:
	system("pause");
	return 0;
}

