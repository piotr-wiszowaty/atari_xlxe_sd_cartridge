#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned char bank8000[8192];
static unsigned char banka000[8192];

static FILE *myfopen(const char *filename, const char *mode)
{
	FILE *fp = fopen(filename, mode);
	if (fp == NULL) {
		perror(filename);
		exit(1);
	}
	return fp;
}

static void slurp(unsigned char *buf, const char *filename)
{
	FILE *fp = myfopen(filename, "rb");
	if (fp == NULL) {
		perror(filename);
		return;
	}
	fread(buf, 1, 8192, fp);
	fclose(fp);
}

static void ascii(const unsigned char *raw, int w, int h)
{
	int y;
	for (y = 0; y < h; y++) {
		int x;
		for (x = 0; x < w; x++) {
			int c = raw[y * w + x] >> 4;
			putchar(" .,:;+=oaeO$@A#M"[c]);
		}
		putchar('\n');
	}
}

static void gr9(unsigned char *p, const unsigned char *line)
{
	int x;
	for (x = 0; x < 160; x += 4) {
		int o = line[x] & 0xf0;
		o |= line[x + 2] >> 4;
		*p++ = o;
	}
}

static void gr10(unsigned char *p, const unsigned char *line)
{
	int x;
	for (x = 1; x < 160; x += 4) {
		int o = (line[x] >> 1) & 0xf0;
		o |= line[x + 2] >> 5;
		*p++ = o;
	}
}

static void hip(const unsigned char *raw, int w, int h)
{
	unsigned char buf[40*240];
	FILE *fp = myfopen("test.hip", "wb");
	int y;
	for (y = 0; y < h; y++)
		gr9(buf + 40 * y, raw + y * w);
	for (y = 0; y < h; y++)
		gr10(buf + 40 * (h + y), raw + y * w);
	fwrite(buf, 1, 40 * 2 * h, fp);
	for (y = 0; y < 16; y += 2)
		putc(y, fp);
	putc(0, fp);
	fclose(fp);
}

static int smp_min = 0x7fff;
static int smp_max = -0x8000;

static void smp(unsigned char *p, FILE *audio)
{
	char buf[312 * 2];
	int i;
	memset(buf, 0, sizeof(buf));
	fread(buf, 1, sizeof(buf), audio);
	for (i = 0; i < 312; i++) {
		int s = (unsigned char) buf[i * 2] + (buf[i * 2 + 1] << 8);
		if (smp_min > s)
			smp_min = s;
		if (smp_max < s)
			smp_max = s;
		s = (s >> 6) + 0x78 + (rand() & 7) >> 3;
		if (s < 0)
			s = 0;
		else if (s > 0x1e)
			s = 0x1e;
		*p++ = 0xe0 | s;
	}
}

static void even(FILE *out, const unsigned char *raw, int w, int h, FILE *audio)
{
	int y;
	for (y = 0; y < h; y++) {
		gr9(bank8000 + 4096 - 40 * h + 80 * y, raw + y * w);
		gr10(bank8000 + 40 + 4096 - 40 * h + 80 * y, raw + y * w);
	}
	smp(bank8000 + 0x1ec0, audio);
	fwrite(bank8000, 1, 8192, out);
}

static void odd(FILE *out, const unsigned char *raw, int w, int h, FILE *audio)
{
	int y;
	for (y = 0; y < h; y++) {
		gr10(banka000 + 4096 - 40 * h + 80 * y, raw + y * w);
		gr9(banka000 + 40 + 4096 - 40 * h + 80 * y, raw + y * w);
	}
	smp(banka000 + 0x1ec0, audio);
	fwrite(banka000, 1, 8192, out);
}

int main(int argc, char *argv[])
{
	FILE *video = myfopen("stream.yuv", "rb");
	FILE *audio = myfopen("audiodump.pcm", "rb");
	FILE *out = myfopen("film.xex", "wb");
	int frame = 0;
	slurp(bank8000, "hip8000.obx");
	slurp(banka000, "hipa000.obx");
	while (getc(video) != '\n');
	for (;;) {
		unsigned char buf[6 + 160 * 90 * 6 / 4];
		if (fread(buf, 1, sizeof(buf), video) != sizeof(buf))
			break;
		if (memcmp(buf, "FRAME\n", 6) != 0) {
			puts("ERROR");
			return 1;
		}
		if (++frame % 100 == 0)
			printf("Frame %d\n", frame);
#if 0
		if (frame == 400) {
			ascii(buf + 6, 160, 90);
			hip(buf + 6, 160, 90);
			break;
		}
#endif
		even(out, buf + 6, 160, 90, audio);
		odd(out, buf + 6, 160, 90, audio);
	}
	fclose(video);
	fclose(audio);
	fclose(out);
	printf("smp min=%d max=%d\n", smp_min, smp_max);
	return 0;
}
