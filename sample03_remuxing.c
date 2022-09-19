#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <stdio.h>

typedef struct _FileContext
{
  AVFormatContext* fmt_ctx;
  int v_index;
  int a_index;
} FileContext;

static FileContext inputFile, outputFile;

static int open_input(const char* fileName)
{
  unsigned int index;

  inputFile.fmt_ctx = NULL;
  inputFile.a_index = inputFile.v_index = -1;

  if(avformat_open_input(&inputFile.fmt_ctx, fileName, NULL, NULL) < 0)
  {
    printf("Could not open input file %s\n", fileName);
    return -1;
  }

  if(avformat_find_stream_info(inputFile.fmt_ctx, NULL) < 0)
  {
    printf("Failed to retrieve input stream information\n");
    return -2;
  }

  for(index = 0; index < inputFile.fmt_ctx->nb_streams; index++)
  {
    AVCodecParameters* avCodecParams = inputFile.fmt_ctx->streams[index]->codecpar;
    if(avCodecParams->codec_type == AVMEDIA_TYPE_VIDEO && inputFile.v_index < 0)
    {
      inputFile.v_index = index;
    }
    else if(avCodecParams->codec_type == AVMEDIA_TYPE_AUDIO && inputFile.a_index < 0)
    {
      inputFile.a_index = index;
    }
  } // for

  if(inputFile.v_index < 0 && inputFile.a_index < 0)
  {
    printf("Failed to retrieve input stream information\n");
    return -3;
  }

  return 0;
}

static int create_output(const char* fileName)
{
  unsigned int index;
  int out_index;

  outputFile.fmt_ctx = NULL;
  outputFile.a_index = outputFile.v_index = -1;

  if(avformat_alloc_output_context2(&outputFile.fmt_ctx, NULL, NULL, fileName) < 0)
  {
    printf("Could not create output context\n");
    return -1;
  }

  // stream index starts from 0.
  out_index = 0;
  // this copy video/audio streams from input video.
  for(index = 0; index < inputFile.fmt_ctx->nb_streams; index++)
  {
    // Make sure we only copy streams which is checked before.
    if(index != inputFile.v_index && index != inputFile.a_index)
    {
      continue;
    }

    AVStream* in_stream = inputFile.fmt_ctx->streams[index];
    AVCodecParameters *in_codecpar = in_stream->codecpar;

    AVStream* out_stream = avformat_new_stream(outputFile.fmt_ctx, NULL);
    if(out_stream == NULL)
    {
      printf("Failed to allocate output stream\n");
      return -2;
    }

    if(avcodec_parameters_copy(out_stream->codecpar, in_codecpar) < 0)
    {
      printf("Error occurred while copying context\n");
      return -3;
    }

    // Remove codec tag info for compatibility with ffmpeg.
    out_stream->codecpar->codec_tag = 0;

    if(index == inputFile.v_index)
    {
      outputFile.v_index = out_index++;
    }
    else
    {
      outputFile.a_index = out_index++;
    }
  } // for

  if(!(outputFile.fmt_ctx->oformat->flags & AVFMT_NOFILE))
  {
    // This actually open the file
    if(avio_open(&outputFile.fmt_ctx->pb, fileName, AVIO_FLAG_WRITE) < 0)
    {
      printf("Failed to create output file %s\n", fileName);
      return -4;
    }
  }

  // write the header for output video container.
  if(avformat_write_header(outputFile.fmt_ctx, NULL) < 0)
  {
    printf("Failed writing header into output file\n");
    return -5;  
  }

  return 0;
}

static void release()
{
  if(inputFile.fmt_ctx != NULL)
  {
    avformat_close_input(&inputFile.fmt_ctx);
  }

  if(outputFile.fmt_ctx != NULL)
  {
    if(!(outputFile.fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
      avio_closep(&outputFile.fmt_ctx->pb);
    }
    avformat_free_context(outputFile.fmt_ctx);
  }
}

int main(int argc, char* argv[])
{
  int ret;

  av_log_set_level(AV_LOG_DEBUG);

  if(argc < 3)
  {
    printf("usage : %s <input> <output>\n", argv[0]);
    return 0;
  }

  if(open_input(argv[1]) < 0)
  {
    goto main_end;
  }

  if(create_output(argv[2]) < 0)
  {
    goto main_end;
  }

  // dump output container, which i just make from above.
  av_dump_format(outputFile.fmt_ctx, 0, outputFile.fmt_ctx->url, 1);

  AVPacket pkt;
  int out_stream_index;

  while(1)
  {
    ret = av_read_frame(inputFile.fmt_ctx, &pkt);
    if(ret == AVERROR_EOF)
    {
      printf("End of frame\n");
      break;
    }

    if(pkt.stream_index != inputFile.v_index && 
      pkt.stream_index != inputFile.a_index)
    {
      av_packet_unref(&pkt);
      continue;
    }

    AVStream* in_stream = inputFile.fmt_ctx->streams[pkt.stream_index];
    out_stream_index = (pkt.stream_index == inputFile.v_index) ? 
            outputFile.v_index : outputFile.a_index;
    AVStream* out_stream = outputFile.fmt_ctx->streams[out_stream_index];

    av_packet_rescale_ts(&pkt, in_stream->time_base, out_stream->time_base);

    pkt.stream_index = out_stream_index;

    if(av_interleaved_write_frame(outputFile.fmt_ctx, &pkt) < 0)
    {
      printf("Error occurred when writing packet into file\n");
      break;
    }   
  } // while

  // Writes remain informations, which it is called trailer.
  av_write_trailer(outputFile.fmt_ctx);

main_end:
  release();

  return 0;
}
