#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>

static enum {
	HIP,
	GR8,
	GR9
} video_mode = HIP;

static const char *audio_volume = "4";

static FILE *xex;
static uint8_t head[8192];
static uint8_t tail[8192];
static uint8_t hip8000[8192];
static uint8_t hipa000[8192];
static uint8_t gr88000[8192];
static uint8_t gr8a000[8192];
static uint8_t gr98000[8192];
static uint8_t gr9a000[8192];
static int audio_pos;
static int video_pos;

static bool open_stream(AVFormatContext *fmt_ctx, const char *name, enum AVMediaType type, int *stream_index, AVCodecContext **dec_ctx)
{
	AVCodec *dec = NULL;
	*stream_index =av_find_best_stream(fmt_ctx, type, -1, -1, &dec, 0);
	if (*stream_index < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find a %s stream in the input file\n", name);
		return false;
	}
	*dec_ctx = fmt_ctx->streams[*stream_index]->codec;
	av_opt_set_int(*dec_ctx, "refcounted_frames", 1, 0);
	if (avcodec_open2(*dec_ctx, dec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open %s decoder\n", name);
		return false;
	}
	return true;
}

static bool parse_graph(AVFilterGraph *filter_graph, AVFilterContext *buffersrc_ctx, AVFilterContext *buffersink_ctx, const char *filters_descr)
{
	AVFilterInOut *outputs = avfilter_inout_alloc();
	AVFilterInOut *inputs  = avfilter_inout_alloc();
	if (outputs == NULL || inputs == NULL) {
		av_log(NULL, AV_LOG_ERROR, "Could not allocate filter\n");
		return false;
	}
	outputs->name       = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx    = 0;
	outputs->next       = NULL;
	inputs->name       = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx    = 0;
	inputs->next       = NULL;
	if (avfilter_graph_parse_ptr(filter_graph, filters_descr, &inputs, &outputs, NULL) < 0)
		return false;
	if (avfilter_graph_config(filter_graph, NULL) < 0)
		return false;
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);
	return true;
}

static bool slurp(unsigned char *buf, const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		perror(filename);
		return false;
	}
	fread(buf, 1, 8192, fp);
	fclose(fp);
	return true;
}

static bool create_xex(const char *input_file)
{
	char output_file[FILENAME_MAX];
	int dot_pos = 0;
	int i;
	for (i = 0; input_file[i] != '\0' && i < FILENAME_MAX - 5; i++)
		if ((output_file[i] = input_file[i]) == '.')
			dot_pos = i;
	strcpy(output_file + (dot_pos == 0 ? i : dot_pos), ".xex");
	xex = fopen(output_file, "wb");
	if (xex == NULL) {
		perror(output_file);
		return false;
	}
	fwrite(head, 1, 8192, xex);
	audio_pos = 0;
	video_pos = 0;
	return true;
}

static int audio_sample(int16_t s)
{
	s = (s + 0x8000 + (rand() & 0xfff)) >> 12;
	if (s < 0)
		return 0xf0;
	if (s > 0xf)
		return 0xff;
	return 0xf0 | s;
}

static void audio_frame(const AVFrame *frame)
{
	int n = frame->nb_samples * av_get_channel_layout_nb_channels(av_frame_get_channel_layout(frame));
	const uint16_t *p = (uint16_t *) frame->data[0];
	int current_pos = -1;
	for (int i = 0; i < n; i++) {
		int desired_pos = (audio_pos / 312 << 13) + 0x1ec0 + audio_pos % 312;
		if (desired_pos != current_pos)
			fseek(xex, desired_pos, SEEK_SET);
		putc(audio_sample(p[i]), xex);
		audio_pos++;
		current_pos = desired_pos + 1;
	}
}

static void gr9_line(const uint8_t *p)
{
	for (int x = 0; x < 160; x += 4) {
		int o = p[x] & 0xf0;
		o |= p[x + 2] >> 4;
		putc(o, xex);
	}
}

static void gr10_line(const uint8_t *p)
{
	for (int x = 1; x < 160; x += 4) {
		int o = p[x] >> 1 & 0xf0;
		o |= p[x + 2] >> 5;
		putc(o, xex);
	}
}

static void video_frame(const AVFrame *frame)
{
	const uint8_t *p = frame->data[0];
	if (video_pos % (60 * 50) == 0) {
		int min = video_pos / (60 * 50);
		printf("%02d:%02d\n", min / 60, min % 60);
	}
	fseek(xex, video_pos << 13, SEEK_SET);
	switch (video_mode) {
	case HIP:
		if ((video_pos & 1) == 0) {
			fwrite(hip8000, 1, 4096 - 40 * 90, xex);
			for (int y = 0; y < 90; y++) {
				gr9_line(p);
				p += frame->linesize[0];
				gr10_line(p);
				p += frame->linesize[0];
			}
			fwrite(hip8000 + 4096 + 40 * 90, 1, 4096 - 40 * 90 - 312 - 8, xex);
			fseek(xex, 312, SEEK_CUR);
			fwrite(hip8000 + 8184, 1, 8, xex);
		}
		else {
			fwrite(hipa000, 1, 4096 - 40 * 90, xex);
			for (int y = 0; y < 90; y++) {
				gr10_line(p);
				p += frame->linesize[0];
				gr9_line(p);
				p += frame->linesize[0];
			}
			fwrite(hipa000 + 4096 + 40 * 90, 1, 4096 - 40 * 90 - 312 - 8, xex);
			fseek(xex, 312, SEEK_CUR);
			fwrite(hipa000 + 8184, 1, 8, xex);
		}
		break;
	case GR8:
		{
			const uint8_t *obx = (video_pos & 1) == 0 ? gr88000 : gr8a000;
			fwrite(obx, 1, 4096 - 40 * 90, xex);
			for (int y = 0; y < 180; y++) {
				fwrite(p, 1, 40, xex);
				p += frame->linesize[0];
			}
			fwrite(obx + 4096 + 40 * 90, 1, 4096 - 40 * 90 - 312 - 8, xex);
			fseek(xex, 312, SEEK_CUR);
			fwrite(obx + 8184, 1, 8, xex);
		}
		break;
	case GR9:
		{
			const uint8_t *obx = (video_pos & 1) == 0 ? gr98000 : gr9a000;
			fwrite(obx, 1, 4096 - 40 * 90, xex);
			for (int y = 0; y < 180; y++) {
				for (int x = 0; x < 80; x += 2) {
					int o = p[x] & 0xf0;
					o |= p[x + 1] >> 4;
					putc(o, xex);
				}
				p += frame->linesize[0];
			}
			fwrite(obx + 4096 + 40 * 90, 1, 4096 - 40 * 90 - 312 - 8, xex);
			fseek(xex, 312, SEEK_CUR);
			fwrite(obx + 8184, 1, 8, xex);
		}
		break;
	}
	video_pos++;
}

static void close_xex()
{
	int len = audio_pos / 312;
	// truncate to full audio+video frames
	if (len > video_pos)
		len = video_pos;
	len &= ~1; // truncate to even number of frames
	len <<= 13;
	fseek(xex, len, SEEK_SET);
	fflush(xex);
	ftruncate(fileno(xex), len);
	fwrite(tail, 1, 8192, xex);
	fclose(xex);
}

static bool encode(const char *input_file)
{
	/* open input */
	AVFrame *frame = av_frame_alloc();
	AVFrame *filt_frame = av_frame_alloc();
	if (frame == NULL || filt_frame == NULL) {
		av_log(NULL, AV_LOG_ERROR, "Could not allocate frame\n");
		return false;
	}
	av_register_all();
	avfilter_register_all();
	AVFormatContext *fmt_ctx = NULL;
	if (avformat_open_input(&fmt_ctx, input_file, NULL, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		return false;
	}
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return false;
	}
	int audio_stream_index;
	AVCodecContext *audio_dec_ctx;
	int video_stream_index;
	AVCodecContext *video_dec_ctx;
	if (!open_stream(fmt_ctx, "audio", AVMEDIA_TYPE_AUDIO, &audio_stream_index, &audio_dec_ctx)
	 || !open_stream(fmt_ctx, "video", AVMEDIA_TYPE_VIDEO, &video_stream_index, &video_dec_ctx))
		return false;

	/* setup audio filter */
	AVFilter *buffersrc  = avfilter_get_by_name("abuffer");
	AVFilter *buffersink = avfilter_get_by_name("abuffersink");
	AVFilterGraph *audio_filter_graph = avfilter_graph_alloc();
	if (audio_filter_graph == NULL) {
		av_log(NULL, AV_LOG_ERROR, "Could not allocate filter\n");
		return false;
	}
	AVRational time_base = fmt_ctx->streams[audio_stream_index]->time_base;
	if (!audio_dec_ctx->channel_layout)
		audio_dec_ctx->channel_layout = av_get_default_channel_layout(audio_dec_ctx->channels);
	char args[512];
	snprintf(args, sizeof(args),
			"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
			 time_base.num, time_base.den, audio_dec_ctx->sample_rate,
			 av_get_sample_fmt_name(audio_dec_ctx->sample_fmt), audio_dec_ctx->channel_layout);
	AVFilterContext *audio_buffersrc_ctx = NULL;
	if (avfilter_graph_create_filter(&audio_buffersrc_ctx, buffersrc, "in", args, NULL, audio_filter_graph) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
		return false;
	}
	AVFilterContext *audio_buffersink_ctx = NULL;
	if (avfilter_graph_create_filter(&audio_buffersink_ctx, buffersink, "out", NULL, NULL, audio_filter_graph) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
		return false;
	}
	static const enum AVSampleFormat out_sample_fmts[] = { AV_SAMPLE_FMT_S16, -1 };
	if (av_opt_set_int_list(audio_buffersink_ctx, "sample_fmts", out_sample_fmts, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
		return false;
	}
    static const int64_t out_channel_layouts[] = { AV_CH_LAYOUT_MONO, -1 };
	if (av_opt_set_int_list(audio_buffersink_ctx, "channel_layouts", out_channel_layouts, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
		return false;
	}
    static const int out_sample_rates[] = { 15600, -1 };
	if (av_opt_set_int_list(audio_buffersink_ctx, "sample_rates", out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
		return false;
	}
	snprintf(args, sizeof(args),
		"aresample=15600,aformat=sample_fmts=s16:channel_layouts=mono,volume=%s",
		audio_volume);
	if (parse_graph(audio_filter_graph, audio_buffersrc_ctx, audio_buffersink_ctx, args) < 0)
		return false;

	/* setup video filter */
	buffersrc  = avfilter_get_by_name("buffer");
	buffersink = avfilter_get_by_name("buffersink");
	AVFilterGraph *video_filter_graph = avfilter_graph_alloc();
	if (video_filter_graph == NULL) {
		av_log(NULL, AV_LOG_ERROR, "Could not allocate filter\n");
		return false;
	}
	snprintf(args, sizeof(args),
			"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
			video_dec_ctx->width, video_dec_ctx->height, video_dec_ctx->pix_fmt,
			video_dec_ctx->time_base.num, video_dec_ctx->time_base.den,
			video_dec_ctx->sample_aspect_ratio.num, video_dec_ctx->sample_aspect_ratio.den);
	AVFilterContext *video_buffersrc_ctx = NULL;
	if (avfilter_graph_create_filter(&video_buffersrc_ctx, buffersrc, "in", args, NULL, video_filter_graph) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
		return false;
	}
	AVFilterContext *video_buffersink_ctx = NULL;
	if (avfilter_graph_create_filter(&video_buffersink_ctx, buffersink, "out", NULL, NULL, video_filter_graph) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
		return false;
	}
	const enum AVPixelFormat pix_fmts[] = { video_mode == GR8 ? AV_PIX_FMT_MONOBLACK : AV_PIX_FMT_GRAY8, AV_PIX_FMT_NONE };
	if (av_opt_set_int_list(video_buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
		return false;
	}
	const char *video_filters_desc =
		video_mode == HIP ? "scale=160:180,fps=50" :
		video_mode == GR8 ? "scale=320:180:sws_dither=ed,fps=50" : // TODO: try sws_dither from https://www.ffmpeg.org/ffmpeg-scaler.html
		video_mode == GR9 ? "scale=80:180,fps=50" :
		NULL;
	if (parse_graph(video_filter_graph, video_buffersrc_ctx, video_buffersink_ctx, video_filters_desc) < 0)
		return false;

	/* encode loop */
	if (!create_xex(input_file))
		return false;
	for (;;) {
		AVPacket packet;
		if (av_read_frame(fmt_ctx, &packet) < 0)
			break;

		if (packet.stream_index == audio_stream_index) {
			AVPacket decoding_packet = packet;
			do {
				int got_frame = 0;
				int ret = avcodec_decode_audio4(audio_dec_ctx, frame, &got_frame, &decoding_packet);
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Error decoding audio\n");
					break;
				}
				decoding_packet.size -= ret;
				decoding_packet.data += ret;
				if (got_frame) {
					if (av_buffersrc_add_frame_flags(audio_buffersrc_ctx, frame, 0) < 0) {
						av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
						break;
					}
					for (;;) {
						ret = av_buffersink_get_frame(audio_buffersink_ctx, filt_frame);
						if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
						    break;
						if (ret < 0) {
							av_log(NULL, AV_LOG_ERROR, "Error filtering audio\n");
							break;
						}
						audio_frame(filt_frame);
						av_frame_unref(filt_frame);
					}
				}
			} while (decoding_packet.size > 0);
		}
		else if (packet.stream_index == video_stream_index) {
			int got_frame = 0;
			if (avcodec_decode_video2(video_dec_ctx, frame, &got_frame, &packet) < 0) {
				av_log(NULL, AV_LOG_ERROR, "Error decoding video\n");
				break;
			}

			if (got_frame) {
				frame->pts = av_frame_get_best_effort_timestamp(frame);
				if (av_buffersrc_add_frame_flags(video_buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
					av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
					break;
				}
				for (;;) {
					int ret = av_buffersink_get_frame(video_buffersink_ctx, filt_frame);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					    break;
					if (ret < 0) {
						av_log(NULL, AV_LOG_ERROR, "Error filtering video\n");
						break;
					}
					video_frame(filt_frame);
					av_frame_unref(filt_frame);
				}
				av_frame_unref(frame);
			}
		}
		av_free_packet(&packet);
	}
	close_xex();

	avfilter_graph_free(&video_filter_graph);
	avfilter_graph_free(&audio_filter_graph);
	avcodec_close(video_dec_ctx);
	avformat_close_input(&fmt_ctx);
	av_frame_free(&filt_frame);
	av_frame_free(&frame);
	return true;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: xexenc INPUTFILE...\n");
		return 1;
	}
	if (!slurp(head, "head.obx")
	 || !slurp(tail, "tail.obx")
	 || !slurp(hip8000, "hip8000.obx")
	 || !slurp(hipa000, "hipa000.obx")
	 || !slurp(gr88000, "gr88000.obx")
	 || !slurp(gr8a000, "gr8a000.obx")
	 || !slurp(gr98000, "gr98000.obx")
	 || !slurp(gr9a000, "gr9a000.obx"))
		return 1;
	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		if (arg[0] == '-') {
			if (strncmp(arg, "--volume=", 9) == 0)
				audio_volume = arg + 9;
			if (strcmp(arg, "--video=hip") == 0)
				video_mode = HIP;
			else if (strcmp(arg, "--video=gr8") == 0)
				video_mode = GR8;
			else if (strcmp(arg, "--video=gr9") == 0)
				video_mode = GR9;
			else {
				fprintf(stderr, "Unknown option: %s\n", arg);
				return 1;
			}
		}
		else if (!encode(arg))
			return 1;
	}
	return 0;
}
