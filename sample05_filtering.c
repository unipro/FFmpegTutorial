#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/avutil.h>
#include <stdio.h>

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

typedef struct _FileContext
{
  AVFormatContext* fmt_ctx;
  AVCodecContext *v_codec_ctx;
  AVCodecContext *a_codec_ctx;
  int v_index;
  int a_index;
} FileContext;

typedef struct _FilterContext
{
  AVFilterGraph* filter_graph;
  AVFilterContext* src_ctx;
  AVFilterContext* sink_ctx;
} FilterContext;

static FileContext inputFile;
static FilterContext vfilter_ctx, afilter_ctx;

static const int dst_width = 480;
static const int dst_height = 320;
static const int64_t dst_ch_layout = AV_CH_LAYOUT_MONO;
static const int dst_sample_rate = 32000;

static int open_decoder(AVCodecContext **codec_ctx, AVStream* stream)
{
  const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
  if(decoder == NULL)
  {
    return -1;
  }

  *codec_ctx = avcodec_alloc_context3(decoder);
  if (!*codec_ctx)
  {
    return -2;
  }

  if(avcodec_parameters_to_context(*codec_ctx, stream->codecpar) < 0)
  {
    return -3;
  }

  if((*codec_ctx)->codec_type == AVMEDIA_TYPE_VIDEO)
  {
    (*codec_ctx)->framerate = av_guess_frame_rate(inputFile.fmt_ctx, stream, NULL);
  }

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

static int init_video_filter()
{
  AVStream* stream = inputFile.fmt_ctx->streams[inputFile.v_index];
  AVCodecContext* codec_ctx = inputFile.v_codec_ctx;
  AVFilterContext* rescale_filter;
  AVFilterInOut *inputs, *outputs;
  char args[512];

  vfilter_ctx.filter_graph = NULL;
  vfilter_ctx.src_ctx = NULL;
  vfilter_ctx.sink_ctx = NULL;

  // Allocate memory for filter graph
  vfilter_ctx.filter_graph = avfilter_graph_alloc();
  if(vfilter_ctx.filter_graph == NULL)
  {
    return -1;
  }

  // Link input and output with filter graph.
  if(avfilter_graph_parse2(vfilter_ctx.filter_graph, "null", &inputs, &outputs) < 0)
  {
    printf("Failed to parse video filtergraph\n");
    return -2;
  }

  // Create input filter
  // Create Buffer Source -> input filter
  snprintf(args, sizeof(args), "time_base=%d/%d:video_size=%dx%d:pix_fmt=%d:pixel_aspect=%d/%d"
    , stream->time_base.num, stream->time_base.den
    , codec_ctx->width, codec_ctx->height
    , codec_ctx->pix_fmt
    , codec_ctx->sample_aspect_ratio.num, codec_ctx->sample_aspect_ratio.den);

  // Create Buffer Source
  if(avfilter_graph_create_filter(
          &vfilter_ctx.src_ctx
          , avfilter_get_by_name("buffer")
          , "in", args, NULL, vfilter_ctx.filter_graph) < 0)
  {
    printf("Failed to create video buffer source\n");
    return -3;
  }

  // Link Buffer Source with input filter
  if(avfilter_link(vfilter_ctx.src_ctx, 0, inputs->filter_ctx, 0) < 0)
  {
    printf("Failed to link video buffer source\n");
    return -4;
  }

  // Create output filter
  // Create Buffer Sink
  if(avfilter_graph_create_filter(
          &vfilter_ctx.sink_ctx
          , avfilter_get_by_name("buffersink")
          , "out", NULL, NULL, vfilter_ctx.filter_graph) < 0)
  {
    printf("Failed to create video buffer sink\n");
    return -3;
  }

  // Create rescaler filter to resize video resolution
  snprintf(args, sizeof(args), "%d:%d", dst_width, dst_height);

  if(avfilter_graph_create_filter(
          &rescale_filter
          , avfilter_get_by_name("scale")
          , "scale", args, NULL, vfilter_ctx.filter_graph) < 0)
  {
    printf("Failed to create video scale filter\n");
    return -4;
  }

  // link rescaler filter with aformat filter
  if(avfilter_link(outputs->filter_ctx, 0, rescale_filter, 0) < 0)
  {
    printf("Failed to link video format filter\n");
    return -4;
  }

  // aformat is linked with Buffer Sink filter.
  if(avfilter_link(rescale_filter, 0, vfilter_ctx.sink_ctx, 0) < 0)
  {
    printf("Failed to link video format filter\n");
    return -4;
  }

  // Configure all prepared filters.
  if(avfilter_graph_config(vfilter_ctx.filter_graph, NULL) < 0)
  {
    printf("Failed to configure video filter context\n");
    return -5;
  }

  av_buffersink_set_frame_size(vfilter_ctx.sink_ctx, codec_ctx->frame_size);

  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  return 0;
}

static int init_audio_filter()
{
  AVStream* stream = inputFile.fmt_ctx->streams[inputFile.a_index];
  AVCodecContext* codec_ctx = inputFile.a_codec_ctx;
  AVFilterInOut *inputs, *outputs;
  AVFilterContext* resample_filter;
  char args[512];
  uint64_t channel_layout;

  afilter_ctx.filter_graph = NULL;
  afilter_ctx.src_ctx = NULL;
  afilter_ctx.sink_ctx = NULL;

  // Allocate memory for filter graph
  afilter_ctx.filter_graph = avfilter_graph_alloc();
  if(afilter_ctx.filter_graph == NULL)
  {
    return -1;
  }

  // Link input and output with filter graph.
  if(avfilter_graph_parse2(afilter_ctx.filter_graph, "anull", &inputs, &outputs) < 0)
  {
    printf("Failed to parse audio filtergraph\n");
    return -2;
  }

  // Create input filter
  // Create Buffer Source -> input filter
  snprintf(args, sizeof(args), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64
    , stream->time_base.num, stream->time_base.den
    , codec_ctx->sample_rate
    , av_get_sample_fmt_name(codec_ctx->sample_fmt)
    , codec_ctx->channel_layout);

  // Create Buffer Source filter
  if(avfilter_graph_create_filter(
          &afilter_ctx.src_ctx
          , avfilter_get_by_name("abuffer")
          , "in", args, NULL, afilter_ctx.filter_graph) < 0)
  {
    printf("Failed to create audio buffer source\n");
    return -3;
  }

  // Link Buffer Source with input filter.
  if(avfilter_link(afilter_ctx.src_ctx, 0, inputs->filter_ctx, 0) < 0)
  {
    printf("Failed to link audio buffer source\n");
    return -4;
  }

  // Create output filter
  // Create Buffer Sink
  if(avfilter_graph_create_filter(
          &afilter_ctx.sink_ctx
          , avfilter_get_by_name("abuffersink")
          , "out", NULL, NULL, afilter_ctx.filter_graph) < 0)
  {
    printf("Failed to create audio buffer sink\n");
    return -3;
  }

  // Create aformat to change audio format.
  snprintf(args, sizeof(args), "sample_rates=%d:channel_layouts=0x%"PRIx64
    , dst_sample_rate
    , dst_ch_layout);

  // Create aformat filter
  if(avfilter_graph_create_filter(
          &resample_filter
          , avfilter_get_by_name("aformat")
          , "aformat", args, NULL, afilter_ctx.filter_graph) < 0)
  {
    printf("Failed to create audio format filter\n");
    return -4;
  }

  // Link output filter with aformat filter
  if(avfilter_link(outputs->filter_ctx, 0, resample_filter, 0) < 0)
  {
    printf("Failed to link audio format filter\n");
    return -4;
  }

  // aformat filter is linked with Buffer Sink.
  if(avfilter_link(resample_filter, 0, afilter_ctx.sink_ctx, 0) < 0)
  {
    printf("Failed to link audio format filter\n");
    return -4;
  }

  // Configure all prepared filters.
  if(avfilter_graph_config(afilter_ctx.filter_graph, NULL) < 0)
  {
    printf("Failed to configure audio filter context\n");
    return -5;
  }

  av_buffersink_set_frame_size(afilter_ctx.sink_ctx, codec_ctx->frame_size);

  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

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

  if(afilter_ctx.filter_graph != NULL)
  {
    avfilter_graph_free(&afilter_ctx.filter_graph);
  }

  if(vfilter_ctx.filter_graph != NULL)
  {
    avfilter_graph_free(&vfilter_ctx.filter_graph);
  }
}

static int decode_packet(AVCodecContext* codec_ctx, AVPacket* pkt, AVFrame* frame)
{
  int ret;

  if(avcodec_send_packet(codec_ctx, pkt) < 0)
  {
    return -1;
  }

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

  if(init_video_filter() < 0)
  {
    goto main_end;
  }

  if(init_audio_filter() < 0)
  {
    goto main_end;
  }

  AVFrame* decoded_frame = av_frame_alloc();
  if(decoded_frame == NULL)
  {
    goto main_end;
  }

  AVFrame* filtered_frame = av_frame_alloc();
  if(filtered_frame == NULL)
  {
    av_frame_free(&decoded_frame);
    goto main_end;
  }
  
  AVPacket pkt;
  int stream_index;

  while(1)
  {
    ret = av_read_frame(inputFile.fmt_ctx, &pkt);
    if(ret == AVERROR_EOF)
    {
      printf("End of frame\n");
      break;
    }

    stream_index = pkt.stream_index;
    AVCodecContext* codec_ctx;

    if(stream_index == inputFile.v_index)
    {
      codec_ctx = inputFile.v_codec_ctx;
    }
    else if(stream_index == inputFile.a_index)
    {
      codec_ctx = inputFile.a_codec_ctx;
    }

    ret = decode_packet(codec_ctx, &pkt, decoded_frame);
    if(ret >= 0)
    {
      FilterContext* filter_ctx;
      
      if(stream_index == inputFile.v_index)
      {
        filter_ctx = &vfilter_ctx;
        printf("[before] Video : resolution : %dx%d\n"
          , decoded_frame->width, decoded_frame->height);
      }
      else if(stream_index == inputFile.a_index)
      {
        filter_ctx = &afilter_ctx;
        printf("[before] Audio : sample_rate : %d / channels : %d\n"
          , decoded_frame->sample_rate, decoded_frame->channels);
      }

      // put frame into filter.
      if(av_buffersrc_add_frame(filter_ctx->src_ctx, decoded_frame) < 0)
      {
        printf("Error occurred when putting frame into filter context\n");
        break;
      }

      while(1)
      {
        // Get frame from filter, if it returns < 0 then filter is currently empty.
        if(av_buffersink_get_frame(filter_ctx->sink_ctx, filtered_frame) < 0)
        {
          break;
        }

        if(stream_index == inputFile.v_index)
        {
          printf("[after] Video : resolution : %dx%d\n"
            , filtered_frame->width, filtered_frame->height);
        }
        else if(stream_index == inputFile.a_index)
        {
          printf("[after] Audio : sample_rate : %d / channels : %d\n"
            , filtered_frame->sample_rate, filtered_frame->channels);
        }

        av_frame_unref(filtered_frame);
      } // while
      av_frame_unref(decoded_frame);
    } // if

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
  av_frame_free(&filtered_frame);

main_end:
  release();
  return 0;
}
