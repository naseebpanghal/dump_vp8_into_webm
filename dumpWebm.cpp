#include <string>
#include <unistd.h>
extern "C" {
#include <libavformat/avformat.h>
}

typedef struct OutputStream {
    AVStream *st;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;
/*ffmpeg*/
AVOutputFormat *fmt;
AVFormatContext *oc;
OutputStream video_st  = { 0 }, audio_st = { 0 };
int have_video = 0, have_audio = 0;
int encode_video = 0, encode_audio = 0;
AVCodec *audio_codec, *video_codec;
std::string filename = { "dump.webm" };
static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
  avcodec_close(ost->st->codec);
}

void webm_deinit()
{
  av_write_trailer(oc);
  if (have_video) {
    close_stream(oc, &video_st);
  }
  if (!(fmt->flags & AVFMT_NOFILE))
    avio_closep(&oc->pb);

  avformat_free_context(oc);
}
static int add_stream(OutputStream *ost, AVFormatContext *oc,
    AVCodec **codec,
    enum AVCodecID codec_id)
{
  AVCodecContext *c;
  int i;

  int retry = 3;
  while(retry)
  {
    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
      printf("file[%s] Could not find encoder for '%s'. Retrying...\n", __FILE__, 
          avcodec_get_name(codec_id));
      usleep(100000);
      retry--;
      continue;
    }
    else
    {
      break;
    }   
  }
  if (!(*codec)) {
    printf("file[%s] Could not find encoder for '%s'.\n", __FILE__,
        avcodec_get_name(codec_id));
    return -1;
  }

  ost->st = avformat_new_stream(oc, *codec);
  if (!ost->st) {
    printf("Could not allocate stream\n");
    return -1;
  }
  ost->st->id = oc->nb_streams-1;
  c = ost->st->codec;

  switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
      c->sample_fmt  = (*codec)->sample_fmts ?
        (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
      c->bit_rate    = 64000;
      c->sample_rate = 48000;
      if ((*codec)->supported_samplerates) {
        c->sample_rate = (*codec)->supported_samplerates[0];
        for (i = 0; (*codec)->supported_samplerates[i]; i++) {
          if ((*codec)->supported_samplerates[i] == 48000)
            c->sample_rate = 48000;
        }
      }
      c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
      c->channel_layout = AV_CH_LAYOUT_STEREO;
      if ((*codec)->channel_layouts) {
        c->channel_layout = (*codec)->channel_layouts[0];
        for (i = 0; (*codec)->channel_layouts[i]; i++) {
          if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
            c->channel_layout = AV_CH_LAYOUT_STEREO;
        }
      }
      c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
      ost->st->time_base = (AVRational){ 1, c->sample_rate };
      break;

    case AVMEDIA_TYPE_VIDEO:
      c->codec_id = codec_id;

      c->bit_rate = 3571/*400000*/;
      /* Resolution must be a multiple of two. */
      c->width    = 1920;
      c->height   = 1080;
      /* timebase: This is the fundamental unit of time (in seconds) in terms
       * of which frame timestamps are represented. For fixed-fps content,
       * timebase should be 1/framerate and timestamp increments should be
       * identical to 1. */
      ost->st->time_base = (AVRational){ 1, 25/*STREAM_FRAME_RATE*/ };
      c->time_base       = ost->st->time_base;

      //c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
      c->pix_fmt       = AV_PIX_FMT_YUV420P;//STREAM_PIX_FMT;
      if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
      }
      if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
        /* Needed to avoid using macroblocks in which some coeffs overflow.
         * This does not happen with normal video, it just happens here as
         * the motion of the chroma plane does not match the luma plane. */
        c->mb_decision = 2;
      }
      break;

    default:
      break;
  }

  /* Some formats want stream headers to be separate. */
  if (oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  return 0;
}

static int open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost)
{
  int ret;
  AVCodecContext *c = ost->st->codec;

  /* open the codec */
  ret = avcodec_open2(c, codec, NULL);
  if (ret < 0) {
    printf("Could not open video codec: %d\n", ret);
    return -1;
  }
  return 0;
}

void webm_init()
{
  int ret;
  /*ffmpeg*/
  /*Register FFMPEG codecs and network functionality*/
  av_register_all();
  avformat_network_init();

  int retry = 3;
  while(retry)
  {
    fmt = av_guess_format("webm", filename.c_str(), "video/webm");
    if(!fmt)
    {
      printf("error av_guess_format\n");
      usleep(100000);
      retry--;
      continue;
    }
    else
    {
      break;
    }
  }

  if(!fmt)
  {
    printf("av_guess_format error even after 3 retry\n");
  }

  fmt->video_codec = AV_CODEC_ID_VP8;
  fmt->audio_codec = AV_CODEC_ID_NONE;

  /* allocate the output media context */
  avformat_alloc_output_context2(&oc, fmt, NULL, NULL);
  if (!oc) {
    printf("Could not deduce output format from file extension\n");
  }

  sprintf(oc->filename, "%s", filename.c_str());

  if (fmt->video_codec != AV_CODEC_ID_NONE) {
    ret = add_stream(&video_st, oc, &video_codec, fmt->video_codec);
    if(ret < 0)
    {
      printf("%s:%d] add_stream failed\n", __func__, __LINE__);
    }
    have_video = 1;
    encode_video = 1;
  }

  if (have_video)
    open_video(oc, video_codec, &video_st);

  av_dump_format(oc, 0, filename.c_str(), 1);

  /* open the output file, if needed */
  if (!(fmt->flags & AVFMT_NOFILE)) {
    ret = avio_open(&oc->pb, filename.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      printf("Could not open '%s': %d\n", filename.c_str(), ret);
    }
  }
}
static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)//, av_type type)
{
  /* rescale output packet timestamp values from codec to stream timebase */
  av_packet_rescale_ts(pkt, *time_base, st->time_base);
  pkt->stream_index = st->index;

  /* Write the compressed frame to the media file. */
  //return av_write_frame(fmt_ctx, pkt);
  return av_interleaved_write_frame(fmt_ctx, pkt);
}
int write_video_frame(AVFormatContext *oc, OutputStream *ost, const unsigned char *data, unsigned int dataLen, bool keyframe)
{
  int retval;
  AVCodecContext *c;
  static bool sbWriteHeaderOnce = true;
  int frame_size;
  AVPacket pkt = { 0 };

  c = ost->st->codec;

  av_new_packet(&pkt, dataLen);
  memcpy(pkt.data, data, dataLen);
  pkt.flags = keyframe; 
  pkt.stream_index = ost->st->index;

  if (sbWriteHeaderOnce)
  { 
    ost->next_pts = 0;
    /* Write the stream header, if any. */
    retval = avformat_write_header(oc, NULL);
    if (retval < 0) {
      printf("Error occurred when opening output file: %d\n", (retval));
      return -1;
    }
    sbWriteHeaderOnce = false;
  }
  pkt.pts = ost->next_pts;
  pkt.dts = AV_NOPTS_VALUE;

  retval = write_frame(oc, &c->time_base, ost->st, &pkt);//, VIDEO);
  av_free_packet(&pkt); 
  if (retval < 0) {
    printf("write_frame error[%d]", (retval));
    return -1;
  }

  ost->next_pts++;
  return 0;
}
void webm_write_frame(const unsigned char *data, unsigned int dataLen, bool keyframe)
{
  write_video_frame(oc, &video_st, data, dataLen, keyframe);
}
