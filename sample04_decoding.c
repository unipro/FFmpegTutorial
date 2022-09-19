#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/avutil.h>
#include <stdio.h>

typedef struct _FileContext
{
  AVFormatContext* fmt_ctx;
  AVCodecContext *v_codec_ctx;
  AVCodecContext *a_codec_ctx;
  int v_index;
  int a_index;
} FileContext;

static FileContext inputFile;

static int open_decoder(AVCodecContext **codec_ctx, AVStream* stream)
{
  // Find a decoder by codec ID
  const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
  if(decoder == NULL)
  {
    return -1;
  }

  // Allocate a codec context for the decoder
  *codec_ctx = avcodec_alloc_context3(decoder);
  if (!*codec_ctx)
  {
    return -2;
  }

  // Copy codec parameters from input stream to output codec context
  if(avcodec_parameters_to_context(*codec_ctx, stream->codecpar) < 0)
  {
    return -3;
  }

  if((*codec_ctx)->codec_type == AVMEDIA_TYPE_VIDEO)
  {
    (*codec_ctx)->framerate = av_guess_frame_rate(inputFile.fmt_ctx, stream, NULL);
  }

  // Open the codec using decoder
  if(avcodec_open2(*codec_ctx, decoder, NULL) < 0)
  {
    return -4;
  }

  return 0;
}

static int open_input(const char* filename)
{
  unsigned int index;
  inputFile.fmt_ctx = NULL;
  inputFile.a_codec_ctx = inputFile.v_codec_ctx = NULL;
  inputFile.a_index = inputFile.v_index = -1;

  if(avformat_open_input(&inputFile.fmt_ctx, filename, NULL, NULL) < 0)
  {
    printf("Could not open input file %s\n", filename);
    return -1;
  }

  if(avformat_find_stream_info(inputFile.fmt_ctx, NULL) < 0)
  {
    printf("Failed to retrieve input stream information\n");
    return -2;
  }

  for(index = 0; index < inputFile.fmt_ctx->nb_streams; index++)
  {
    AVStream* stream = inputFile.fmt_ctx->streams[index];
    if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && inputFile.v_index < 0)
    {
      if(open_decoder(&inputFile.v_codec_ctx, stream) < 0)
      {
        break;
      }

      inputFile.v_index = index;
    }
    else if(stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && inputFile.a_index < 0)
    {
      if(open_decoder(&inputFile.a_codec_ctx, stream) < 0)
      {
        break;
      }

      inputFile.a_index = index;
    }
  } // for

  if(inputFile.a_index < 0 && inputFile.a_index < 0)
  {
    printf("Failed to retrieve input stream information\n");
    return -3;
  }

  return 0;
}

static void release()
{
  if(inputFile.v_codec_ctx != NULL)
  {
    avcodec_close(inputFile.v_codec_ctx);
  }

  if(inputFile.a_codec_ctx != NULL)
  {
    avcodec_close(inputFile.a_codec_ctx);
  }

  if(inputFile.fmt_ctx != NULL)
  {
    avformat_close_input(&inputFile.fmt_ctx);
  }
}

static int decode_packet(AVCodecContext* codec_ctx, AVPacket* pkt, AVFrame* frame)
{
  int ret;

  // submit the packet to the decoder
  if(avcodec_send_packet(codec_ctx, pkt) < 0)
  {
    return -1;
  }

  // Get all the available frames from the decoder
  ret = avcodec_receive_frame(codec_ctx, frame);
  if(ret < 0)
  {
    return ret;
  }

  return 0;
}

int main(int argc, char* argv[])
{
  int ret;

  av_log_set_level(AV_LOG_DEBUG);

  if(argc < 2)
  {
    printf("usage : %s <input>\n", argv[0]);
    return 0;
  }

  if(open_input(argv[1]) < 0)
  {
    goto main_end;
  }

  // AVFrame is used to store raw frame, which is decoded from packet.
  AVFrame* decoded_frame = av_frame_alloc();
  if(decoded_frame == NULL) goto main_end;
  
  AVPacket pkt;

  while(1)
  {
    ret = av_read_frame(inputFile.fmt_ctx, &pkt);
    if(ret == AVERROR_EOF)
    {
      printf("End of frame\n");
      break;
    }

    if(pkt.stream_index == inputFile.v_index)
    {
      ret = decode_packet(inputFile.v_codec_ctx, &pkt, decoded_frame);
      if (ret >= 0)
      {
        printf("-----------------------\n");
        printf("Video : frame->width, height : %dx%d\n",
          decoded_frame->width, decoded_frame->height);
        printf("Video : frame->sample_aspect_ratio : %d/%d\n",
          decoded_frame->sample_aspect_ratio.num, decoded_frame->sample_aspect_ratio.den);
        av_frame_unref(decoded_frame);
      }
    }
    else if(pkt.stream_index == inputFile.a_index)
    {
      ret = decode_packet(inputFile.a_codec_ctx, &pkt, decoded_frame);
      if (ret >= 0)
      {
        printf("-----------------------\n");
        printf("Audio : frame->nb_samples : %d\n",
          decoded_frame->nb_samples);
        printf("Audio : frame->channels : %d\n",
          decoded_frame->channels);
        av_frame_unref(decoded_frame);
      }
    }

    av_packet_unref(&pkt);
  } // while

  // flush the decoder
  if(inputFile.v_codec_ctx != NULL)
  {
    decode_packet(inputFile.v_codec_ctx, NULL, decoded_frame);
  }
  if(inputFile.a_codec_ctx != NULL)
  {
    decode_packet(inputFile.a_codec_ctx, NULL, decoded_frame);
  }

  av_frame_free(&decoded_frame);

main_end:
  release();

  return 0;
}
