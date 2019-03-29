#include <stdio.h>
#include <iostream>

#define __STDC_CONSTANT_MACROS

#define AV_CODEC_FLAG_GLOBAL_HEADER (1 << 22)
#define CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
#define AVFMT_RAWPICTURE 0x0020

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"

#include <WinSock2.h>

#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/audio_fifo.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#ifdef __cplusplus
};
#endif
#endif


#pragma comment(lib,"ws2_32.lib")
using namespace std;

int main(int argc, char* argv[])
{
    WSADATA wsadata;
    SOCKET serverSocket;
    int str_len,szClientAddr, clientAddr;
    SOCKADDR_IN  serverAddr;

    //char message[bufsize] = "\0";

    if(WSAStartup(MAKEWORD(2, 2), &wsadata)!=0)
            cout<<"WSAStartup() error"<<endl;

    serverSocket = socket(PF_INET, SOCK_DGRAM, 0);
    if(serverSocket == INVALID_SOCKET)
        cout<<"socket()  error"<<endl;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(9999);

    if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        cout << "bind () error" << endl;
    AVFormatContext* pInputFormatContext=NULL;
    AVCodec* pInputCodec=NULL;
    AVCodecContext* pInputCodecContex=NULL;

    AVFormatContext *pOutputFormatContext=NULL;
    AVCodecContext* pOutCodecContext=NULL;
    AVCodec* pOutCodec=NULL;
    AVStream* pOutStream=NULL;


    // init part
    av_register_all();
    avformat_network_init();
    avdevice_register_all();
    avcodec_register_all();


    const char* out_file = "udp://192.168.1.109:1997";

    int ret,i;
    int videoindex=-1;
    //输入（Input）
    pInputFormatContext = avformat_alloc_context();
    AVDictionary* options = NULL;
    AVInputFormat *ifmt=av_find_input_format("gdigrab");
    //av_dict_set(&options,"framerate","25",0);
    //av_dict_set(&options,"video_size","1440x900",0);
    if(avformat_open_input(&pInputFormatContext,"desktop",ifmt,&options)!=0){ //Grab at position 10,20 真正的打开文件,这个函数读取文件的头部并且把信息保存到我们给的AVFormatContext结构体中
        printf("Couldn't open input stream.\n");
        return -1;
    }
    if ((ret = avformat_find_stream_info(pInputFormatContext, 0)) < 0) {
        printf( "Failed to retrieve input stream information");
        return -1;
    }

    for(i=0; i < pInputFormatContext->nb_streams; i++)
        if(pInputFormatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
            videoindex=i;
            break;
        }

    pInputCodecContex=pInputFormatContext->streams[videoindex]->codec;
    // 相当于虚基类,具体参数流中关于编解码器的信息就是被我们叫做"codec context"（编解码器上下文）的东西。
    pInputCodec=avcodec_find_decoder(pInputCodecContex->codec_id);
    //这里面包含了流中所使用的关于编解码器的所有信息，现在我们有了一个指向他的指针。但是我们必需要找到真正的编解码器并且打开
    if(pInputCodec==NULL)
    {
        printf("Codec not found.\n");
        return -1;
    }
    //打开解码器
    if(avcodec_open2(pInputCodecContex, pInputCodec,NULL)<0)
    {
        printf("Could not open codec.\n");
        return -1;
    }
    //为一帧图像分配内存
    std::cout<<"picture width height format"<<pInputCodecContex->width<<pInputCodecContex->height<<pInputCodecContex->pix_fmt<<std::endl;
    AVFrame *pFrame;
    AVFrame *pFrameYUV;
    pFrame=av_frame_alloc();
    pFrameYUV=av_frame_alloc();//为转换来申请一帧的内存(把原始帧->YUV)

    pFrameYUV->format=AV_PIX_FMT_YUV420P;
    pFrameYUV->width=pInputCodecContex->width;
    pFrameYUV->height=pInputCodecContex->height;
    unsigned char *out_buffer=(unsigned char *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pInputCodecContex->width, pInputCodecContex->height));
    //现在我们使用avpicture_fill来把帧和我们新申请的内存来结合
    avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, pInputCodecContex->width, pInputCodecContex->height);
    

    struct SwsContext *img_convert_ctx;
    img_convert_ctx = sws_getContext(pInputCodecContex->width, pInputCodecContex->height, pInputCodecContex->pix_fmt, pInputCodecContex->width, pInputCodecContex->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
//=============================================================================================
    avformat_alloc_output_context2(&pOutputFormatContext, NULL, "h264", out_file); //RTMP
    if (!pOutputFormatContext) {
        printf( "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        return -1;
    }
    //Open output URL 打开输入文件 返回AVIOContext(pFormatCtx->pb)
    if (avio_open(&pOutputFormatContext->pb,out_file, AVIO_FLAG_READ_WRITE) < 0){
        printf("Failed to open output file! \n");
        return -1;
    }

    //创建输出流,AVStream  与  AVCodecContext一一对应
    pOutStream = avformat_new_stream(pOutputFormatContext, 0);
    if(pOutStream == NULL)
    {
        printf("Failed create pOutStream!\n");
        return -1;
    }
    //相当于虚基类,具体参数
    pOutCodecContext = pOutStream->codec;

    pOutCodecContext->codec_id = AV_CODEC_ID_H264;
    //type
    pOutCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    //像素格式,
    pOutCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    //size
    pOutCodecContext->width = pInputCodecContex->width;
    pOutCodecContext->height = pInputCodecContex->height;
    //目标码率
    pOutCodecContext->bit_rate = 400000;
    //每250帧插入一个I帧,I帧越小视频越小
    pOutCodecContext->gop_size=100;
    //Optional Param B帧
    pOutCodecContext->max_b_frames=3;

    pOutCodecContext->time_base.num = 1;
    pOutCodecContext->time_base.den = 25;

    //pOutCodecContext->lmin=1;
    //pOutCodecContext->lmax=50;
    //最大和最小量化系数
    pOutCodecContext->qmin = 10;
    pOutCodecContext->qmax = 51;
    pOutCodecContext->qblur=0.0;

    av_dump_format(pOutputFormatContext, 0, out_file, 1);

    AVDictionary *param = 0;
//    //H.264
    if(pOutCodecContext->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "slow", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }
//
    av_dump_format(pOutputFormatContext, 0, out_file, 1);

    pOutCodec = avcodec_find_encoder(pOutCodecContext->codec_id);
    if (!pOutCodec){
        printf("Can not find encoder! \n");
        return -1;
    }

    if (avcodec_open2(pOutCodecContext, pOutCodec,&param) < 0)
    {
        printf("Failed to open encoder! \n");
        return -1;
    }
    //Write File Header
   int r = avformat_write_header(pOutputFormatContext,NULL);
   if(r<0)
   {
        printf("Failed write header!\n");
        return -1;
   }

   AVPacket *packet=(AVPacket *)av_malloc(sizeof(AVPacket));
   int got_picture;

   AVPacket pkt;
   int picture_size = avpicture_get_size(pOutCodecContext->pix_fmt, pOutCodecContext->width, pOutCodecContext->height);
   av_new_packet(&pkt,picture_size);

   int frame_index=0;
   while((av_read_frame(pInputFormatContext, packet))>=0)
   {
       if(packet->stream_index==videoindex)
       {
           //真正解码,packet to pFrame
           avcodec_decode_video2(pInputCodecContex, pFrame, &got_picture, packet);

           if(got_picture)
           {
                   sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pInputCodecContex->height, pFrameYUV->data, pFrameYUV->linesize);
                   pFrameYUV->pts=frame_index;
                   frame_index++;
                   int picture;
                   //真正编码
                   int ret=avcodec_encode_video2(pOutCodecContext, &pkt,pFrameYUV, &picture);
                   if(ret < 0){
                       printf("Failed to encode! \n");
                       return -1;
                   }
                   if (picture==1)
                   {
                       ret = av_interleaved_write_frame(pOutputFormatContext, &pkt);

                       if (ret < 0) {
                           printf( "Error muxing packet\n");
                           break;
                       }

                       av_free_packet(&pkt);
                   }
           }
       }
       av_free_packet(packet);
   }

    //Write file trailer
    av_write_trailer(pOutputFormatContext);

        sws_freeContext(img_convert_ctx);
        //fclose(fp_yuv);
        av_free(out_buffer);
        av_free(pFrameYUV);
        av_free(pFrame);
        avcodec_close(pInputCodecContex);
        avformat_close_input(&pInputFormatContext);

    avcodec_close(pOutStream->codec);
    av_free(pOutCodec);
    avcodec_close(pOutCodecContext);
    avformat_free_context(pOutputFormatContext);

    return 0;
}
