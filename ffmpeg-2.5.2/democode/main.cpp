//Windows下使用FFMPEG解码视频并保存成图片文件的简单的例子
/**
 * 叶海辉
 * QQ群121376426
 * http://blog.yundiantech.com/
 */

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavutil/pixfmt.h"
    #include "libswscale/swscale.h"
}

#include <stdio.h>

//现在我们需要做的是让SaveFrame函数能把RGB信息定稿到一个PPM格式的文件中。
//我们将生成一个简单的PPM格式文件，请相信，它是可以工作的。
void SaveFrame(AVFrame *pFrame, int width, int height,int index)
{

  FILE *pFile;
  char szFilename[32];
  int  y;

  // Open file
  sprintf(szFilename, "frame%d.ppm", index);
  pFile=fopen(szFilename, "wb");

  if(pFile==NULL)
    return;

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  for(y=0; y<height; y++)
  {
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
  }

  // Close file
  fclose(pFile);

}


int main(int argc, char *argv[])
{
    char *file_path = "E:\\in.mp4";

    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame, *pFrameRGB;
    AVPacket *packet;
    uint8_t *out_buffer;

    static struct SwsContext *img_convert_ctx;

    int videoStream, i, numBytes;
    int ret, got_picture;
//1.初始化FFMPEG
    av_register_all(); //初始化FFMPEG  调用了这个才能正常适用编码器和解码器

    //Allocate an AVFormatContext.
    pFormatCtx = avformat_alloc_context();
//2.打开输入文件并读取头部信息保存到AVFormatContext
    if (avformat_open_input(&pFormatCtx, file_path, NULL, NULL) != 0) {
        printf("can't open the file. \n");
        return -1;
    }
//3.读包获取流信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("Could't find stream infomation.\n");
        return -1;
    }

    videoStream = -1;
//4.根据流信息分离音频视频
    ///循环查找视频中包含的流信息，直到找到视频类型的流
    ///便将其记录下来 保存到videoStream变量中
    ///这里我们现在只处理视频流  音频流先不管他
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
        }
    }

    ///如果videoStream为-1 说明没有找到视频流
    if (videoStream == -1) {
        printf("Didn't find a video stream.\n");
        return -1;
    }
//5.分离音频视频后查找对应的解码器
    //查找解码器
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if (pCodec == NULL) {
        printf("Codec not found.\n");
        return -1;
    }
//6.打开解码器
    ///打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }

    pFrame = av_frame_alloc();//源帧
    pFrameRGB = av_frame_alloc();//目的帧
//7.为图片缩放申请一个SwsContext结构体
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
            pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
            PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

    numBytes = avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width,pCodecCtx->height);

    out_buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
//8.关联AVPicture与数据
    avpicture_fill((AVPicture *) pFrameRGB, out_buffer, PIX_FMT_RGB24,
            pCodecCtx->width, pCodecCtx->height);

    int y_size = pCodecCtx->width * pCodecCtx->height;

    packet = (AVPacket *) malloc(sizeof(AVPacket)); //分配一个packet
//9.分配负荷缓存区
//为AVPacket结构体分配负载缓存区并初始化，这个负载缓存区大小为size
    av_new_packet(packet, y_size); //分配packet的数据

    av_dump_format(pFormatCtx, 0, file_path, 0); //输出视频信息

    int index = 0;

    while (1)
    {//10.读包数据
        if (av_read_frame(pFormatCtx, packet) < 0)
        {
            break; //这里认为视频读取完了
        }

        if (packet->stream_index == videoStream) {
	//11.对包数据解码
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture,packet);

            if (ret < 0) {
                printf("decode error.\n");
                return -1;
            }
			//got_picture为1表示有数据解码，所以要处理
			//got_picture为0表示没有数据解码，所以跳过if不处理
            if (got_picture) {
	//12.对解码后的数据缩放
                sws_scale(img_convert_ctx,
                        (uint8_t const * const *) pFrame->data,
                        pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data,
                        pFrameRGB->linesize);

                SaveFrame(pFrameRGB, pCodecCtx->width,pCodecCtx->height,index++); //保存图片
                if (index > 50) return 0; //这里我们就保存50张图片
            }
        }
		//逆操作，释放packet
        av_free_packet(packet);
    }
	//13.逆操作，作清理工作
	//逆操作，释放out_buffer
    av_free(out_buffer);
	//逆操作，释放pFrameRGB
    av_free(pFrameRGB);
	//逆操作，关闭解码器
    avcodec_close(pCodecCtx);
	//逆操作，关闭pFormatCtx
    avformat_close_input(&pFormatCtx);

    return 0;
}

