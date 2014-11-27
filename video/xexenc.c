#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>

#define STRINGIFY0(x) #x
#define STRINGIFY(x) STRINGIFY0(x)

#define U_BITS 6 // 3 (fast) to 8 (highest quality)
#define Y_BITS 6 // 4 (fast) to 8 (highest quality)
#define Y_WEIGHT 1

#if NTSC
#define FPS 60
#define SCANLINES 262
#else
#define FPS 50
#define SCANLINES 312
#endif

#define HEAD_LEN 0 // TODO: 8192

static enum {
	TIP,
	HIP,
	GR8,
	GR9
} video_mode = TIP;

static const char *output_file = NULL;
static const char *audio_volume = "4";

static FILE *xex;
static uint8_t ataripal[768];
static uint8_t head[8192];
static uint8_t tail[8192];
static uint8_t tip8000[8192];
static uint8_t tipa000[8192];
static uint8_t hip8000[8192];
static uint8_t hipa000[8192];
static uint8_t gr88000[8192];
static uint8_t gr8a000[8192];
static uint8_t gr98000[8192];
static uint8_t gr9a000[8192];
static int audio_pos;
static int video_pos;
static uint8_t uvy2atari[1 << (U_BITS * 2 + Y_BITS)];

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
		*stream_index = -1;
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

static bool slurp(unsigned char *buf, size_t len, const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		perror(filename);
		return false;
	}
	fread(buf, 1, len, fp);
	fclose(fp);
	return true;
}

static bool create_xex(const char *input_file)
{
	char output_default[FILENAME_MAX];
	if (output_file == NULL) {
		int dot_pos = 0;
		int i;
		for (i = 0; input_file[i] != '\0' && i < FILENAME_MAX - 5; i++)
			if ((output_default[i] = input_file[i]) == '.')
				dot_pos = i;
		strcpy(output_default + (dot_pos == 0 ? i : dot_pos), ".xex");
		output_file = output_default;
	}
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
		int desired_pos = HEAD_LEN + (audio_pos / SCANLINES << 13) + 0x1ff8 - SCANLINES + audio_pos % SCANLINES;
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

static void tip_init(void)
{
	uint8_t yuvpal[256 * 3];
	for (int c = 0; c < 256; c++) {
		uint8_t r = ataripal[c * 3];
		uint8_t g = ataripal[c * 3 + 1];
		uint8_t b = ataripal[c * 3 + 2];
		int y =       ( 299 * r + 587 * g + 114 * b) / 1000;
		int u = 128 + (-169 * r - 331 * g + 500 * b) / 1000;
		int v = 128 + ( 500 * b - 419 * g -  81 * b) / 1000;
		if (y < 0 || y > 255 || u < 0 || y > 255 || v < 0 || v > 255) {
			av_log(NULL, AV_LOG_ERROR, "Color out of range: color=%02x y=%d u=%d v=%d\n", c, y, u, v);
			exit(1);
		}
		yuvpal[c * 3] = y;
		yuvpal[c * 3 + 1] = u;
		yuvpal[c * 3 + 2] = v;
	}

	for (int i = 0; i < sizeof(uvy2atari); i++) {
		uint8_t u = i >> (U_BITS + Y_BITS) << (8 - U_BITS);
		uint8_t v = (i >> Y_BITS) << (8 - U_BITS);
		uint8_t y = i << (8 - Y_BITS);
		int best_dist = 1 << 30;
		int best_c = 0;
		for (int c = 0; c < 256; c++) {
			int dy = y - yuvpal[c * 3];
			int du = u - yuvpal[c * 3 + 1];
			int dv = v - yuvpal[c * 3 + 2];
			int dist = Y_WEIGHT * dy * dy + du * du + dv * dv;
			if (dist < best_dist) {
				best_dist = dist;
				best_c = c;
			}
		}
		uvy2atari[i] = (uint8_t) best_c;
	}
}

static void tip_line(const uint8_t *p, int gr10)
{
	uint8_t shades[40];
	gr10 <<= 1;
	for (int x = 0; x < 320; x += 8) {
		int atari_left = uvy2atari[(((p[x + 1] >> (8 - U_BITS) << U_BITS) + (p[x + 3] >> (8 - U_BITS))) << Y_BITS) + (p[x + gr10] >> (8 - Y_BITS))];
		int atari_right = uvy2atari[(((p[x + 5] >> (8 - U_BITS) << U_BITS) + (p[x + 7] >> (8 - U_BITS))) << Y_BITS) + (p[x + gr10 + 4] >> (8 - Y_BITS))];
		putc((atari_left & 0xf0) | atari_right >> 4, xex);
		int o = atari_left << 4 | (atari_right & 0xf);
		shades[x >> 3] = gr10 == 0 ? o : o >> 1 & 0x77;
	}
	fwrite(shades, 1, 40, xex);
}

static void video_frame(const AVFrame *frame)
{
	const uint8_t *p = frame->data[0];
	if (video_pos % (60 * FPS) == 0) {
		int min = video_pos / (60 * FPS);
		printf("%02d:%02d\n", min / 60, min % 60);
	}
	fseek(xex, HEAD_LEN + (video_pos << 13), SEEK_SET);
	const uint8_t *obx = NULL;
	switch (video_mode) {
	case TIP:
		{
			static bool initialized = false;
			if (!initialized) {
				tip_init();
				initialized = true;
			}
		}
		obx = (video_pos & 1) == 0 ? tip8000 : tipa000;
		fwrite(obx, 1, 4096 - 40 * 90, xex);
		for (int y = 0; y < 90; y++) {
			tip_line(p, (y ^ video_pos) & 1);
			p += frame->linesize[0];
		}
		break;
	case HIP:
		obx = (video_pos & 1) == 0 ? hip8000 : hipa000;
		fwrite(obx, 1, 4096 - 40 * 90, xex);
		for (int y = 0; y < 180; y++) {
			if (((y ^ video_pos) & 1) == 0)
				gr9_line(p);
			else
				gr10_line(p);
			p += frame->linesize[0];
		}
		break;
	case GR8:
		obx = (video_pos & 1) == 0 ? gr88000 : gr8a000;
		fwrite(obx, 1, 4096 - 40 * 90, xex);
		for (int y = 0; y < 180; y++) {
			fwrite(p, 1, 40, xex);
			p += frame->linesize[0];
		}
		break;
	case GR9:
		obx = (video_pos & 1) == 0 ? gr98000 : gr9a000;
		fwrite(obx, 1, 4096 - 40 * 90, xex);
		for (int y = 0; y < 180; y++) {
			for (int x = 0; x < 80; x += 2) {
				int o = p[x] & 0xf0;
				o |= p[x + 1] >> 4;
				putc(o, xex);
			}
			p += frame->linesize[0];
		}
		break;
	}
	fwrite(obx + 4096 + 40 * 90, 1, 4096 - 40 * 90 - SCANLINES - 8, xex);
	fseek(xex, SCANLINES, SEEK_CUR);
	fwrite(obx + 8184, 1, 8, xex);
	video_pos++;
}

static void close_xex(void)
{
	int len = audio_pos / SCANLINES;
	// truncate to full audio+video frames
	if (audio_pos == 0 || len > video_pos)
		len = video_pos;
	len &= ~1; // truncate to even number of frames
	len = HEAD_LEN + (len << 13);
	fseek(xex, len, SEEK_SET);
	fflush(xex);
	ftruncate(fileno(xex), len);
	fwrite(tail, 1, 8192, xex);
	fclose(xex);
	output_file = NULL;
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

	/* setup audio filter */
	int audio_stream_index;
	AVCodecContext *audio_dec_ctx;
	AVFilterGraph *audio_filter_graph = NULL;
	AVFilterContext *audio_buffersrc_ctx = NULL;
	AVFilterContext *audio_buffersink_ctx = NULL;
	if (open_stream(fmt_ctx, "audio", AVMEDIA_TYPE_AUDIO, &audio_stream_index, &audio_dec_ctx)) {
		AVFilter *buffersrc  = avfilter_get_by_name("abuffer");
		AVFilter *buffersink = avfilter_get_by_name("abuffersink");
		audio_filter_graph = avfilter_graph_alloc();
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
		if (avfilter_graph_create_filter(&audio_buffersrc_ctx, buffersrc, "in", args, NULL, audio_filter_graph) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
			return false;
		}
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
		static const int out_sample_rates[] = { FPS * SCANLINES, -1 };
		if (av_opt_set_int_list(audio_buffersink_ctx, "sample_rates", out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
			return false;
		}
		snprintf(args, sizeof(args),
			"aresample=%d,aformat=sample_fmts=s16:channel_layouts=mono,volume=%s",
			FPS * SCANLINES, audio_volume);
		if (parse_graph(audio_filter_graph, audio_buffersrc_ctx, audio_buffersink_ctx, args) < 0)
			return false;
	}

	/* setup video filter */
	int video_stream_index;
	AVCodecContext *video_dec_ctx;
	AVFilterGraph *video_filter_graph;
	AVFilterContext *video_buffersrc_ctx = NULL;
	AVFilterContext *video_buffersink_ctx = NULL;
	if (!open_stream(fmt_ctx, "video", AVMEDIA_TYPE_VIDEO, &video_stream_index, &video_dec_ctx))
		return false;
	{
		AVFilter *buffersrc  = avfilter_get_by_name("buffer");
		AVFilter *buffersink = avfilter_get_by_name("buffersink");
		video_filter_graph = avfilter_graph_alloc();
		if (video_filter_graph == NULL) {
			av_log(NULL, AV_LOG_ERROR, "Could not allocate filter\n");
			return false;
		}
		char args[512];
		snprintf(args, sizeof(args),
				"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
				video_dec_ctx->width, video_dec_ctx->height, video_dec_ctx->pix_fmt,
				video_dec_ctx->time_base.num, video_dec_ctx->time_base.den,
				video_dec_ctx->sample_aspect_ratio.num, video_dec_ctx->sample_aspect_ratio.den);
		if (avfilter_graph_create_filter(&video_buffersrc_ctx, buffersrc, "in", args, NULL, video_filter_graph) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
			return false;
		}
		if (avfilter_graph_create_filter(&video_buffersink_ctx, buffersink, "out", NULL, NULL, video_filter_graph) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
			return false;
		}
		enum AVPixelFormat pix_fmts[2] = { AV_PIX_FMT_NONE, AV_PIX_FMT_NONE };
		const char *video_filters_desc;
		switch (video_mode) {
		case TIP: pix_fmts[0] = AV_PIX_FMT_YUYV422;   video_filters_desc = "scale=160:90,fps=" STRINGIFY(FPS);  break;
		case HIP: pix_fmts[0] = AV_PIX_FMT_GRAY8;     video_filters_desc = "scale=160:180,fps=" STRINGIFY(FPS); break;
		case GR8: pix_fmts[0] = AV_PIX_FMT_MONOBLACK; video_filters_desc = "scale=320:180:sws_dither=ed,fps=" STRINGIFY(FPS); break; // TODO: try sws_dither from https://www.ffmpeg.org/ffmpeg-scaler.html
		case GR9: pix_fmts[0] = AV_PIX_FMT_GRAY8;     video_filters_desc = "scale=80:180,fps=" STRINGIFY(FPS); break;
		default:
			fprintf(stderr, "Unknown video mode %d\n", video_mode);
			return false;
		}
		if (av_opt_set_int_list(video_buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
			return false;
		}
		if (parse_graph(video_filter_graph, video_buffersrc_ctx, video_buffersink_ctx, video_filters_desc) < 0)
			return false;
	}

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

static void usage(void)
{
	printf(
		"Usage: xexenc [OPTIONS] INPUTFILE...\n"
		"Options:\n"
		"--output=FILE    Set output filename (default: INPUTFILE with .xex extension)\n"
		"--volume=DECIMAL Set audio volume (default: 4.0)\n"
		"--video=tip      Set TIP (160x90, 256 colors) graphics mode (default)\n"
		"--video=hip      Set HIP (160x180, 16 levels of gray) graphics mode\n"
		"--video=gr8      Set GR8 (320x180, mono) graphics mode\n"
		"--video=gr9      Set GR9 (80x180, 16 levels of gray) graphics mode\n"
		"--help           Display this information\n"
	);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage();
		return 1;
	}
	if (!slurp(ataripal, sizeof(ataripal), "altirrapal.pal")
	 || !slurp(head, sizeof(head), "head.obx")
	 || !slurp(tail, sizeof(tail), "tail.obx")
	 || !slurp(tip8000, sizeof(tip8000), "tip8000.obx")
	 || !slurp(tipa000, sizeof(tipa000), "tipa000.obx")
	 || !slurp(hip8000, sizeof(hip8000), "hip8000.obx")
	 || !slurp(hipa000, sizeof(hipa000), "hipa000.obx")
	 || !slurp(gr88000, sizeof(gr88000), "gr88000.obx")
	 || !slurp(gr8a000, sizeof(gr8a000), "gr8a000.obx")
	 || !slurp(gr98000, sizeof(gr98000), "gr98000.obx")
	 || !slurp(gr9a000, sizeof(gr9a000), "gr9a000.obx"))
		return 1;
	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		if (arg[0] == '-') {
			if (strncmp(arg, "--output=", 9) == 0)
				output_file = arg + 9;
			else if (strncmp(arg, "--volume=", 9) == 0)
				audio_volume = arg + 9;
			else if (strcmp(arg, "--video=tip") == 0)
				video_mode = TIP;
			else if (strcmp(arg, "--video=hip") == 0)
				video_mode = HIP;
			else if (strcmp(arg, "--video=gr8") == 0)
				video_mode = GR8;
			else if (strcmp(arg, "--video=gr9") == 0)
				video_mode = GR9;
			else if (strcmp(arg, "--help") == 0)
				usage();
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
