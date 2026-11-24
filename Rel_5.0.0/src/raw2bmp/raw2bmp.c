#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

static void write_u16(FILE *fp, uint16_t value)
{
    fputc(value & 0xFF, fp);
    fputc((value >> 8) & 0xFF, fp);
}

static void write_u32(FILE *fp, uint32_t value)
{
    fputc(value & 0xFF, fp);
    fputc((value >> 8) & 0xFF, fp);
    fputc((value >> 16) & 0xFF, fp);
    fputc((value >> 24) & 0xFF, fp);
}

static int parse_positive(const char *text, int *result)
{
    char *end;
    long value;

    value = strtol(text, &end, 10);

    if (*text == '\0' || *end != '\0' ||
        value <= 0 || value > INT_MAX)
        return 0;

    *result = (int)value;
    return 1;
}

static int write_bmp(const unsigned char *pixels,
                     int width, int height)
{
    uint32_t row_size;
    uint32_t image_size;
    uint32_t file_size;
    int padding;
    int y, x;

    row_size = ((uint32_t)width + 3U) & ~3U;
    image_size = row_size * (uint32_t)height;
    file_size = 14U + 40U + 1024U + image_size;
    padding = (int)(row_size - (uint32_t)width);

    /* BMP file header */
    fputc('B', stdout);
    fputc('M', stdout);
    write_u32(stdout, file_size);
    write_u16(stdout, 0);
    write_u16(stdout, 0);
    write_u32(stdout, 1078);

    /* DIB header: BITMAPINFOHEADER */
    write_u32(stdout, 40);
    write_u32(stdout, (uint32_t)width);
    write_u32(stdout, (uint32_t)height);
    write_u16(stdout, 1);
    write_u16(stdout, 8);
    write_u32(stdout, 0);
    write_u32(stdout, image_size);
    write_u32(stdout, 2835);
    write_u32(stdout, 2835);
    write_u32(stdout, 256);
    write_u32(stdout, 256);

    /* 256-entry grayscale palette */
    for (x = 0; x < 256; x++) {
        fputc(x, stdout);
        fputc(x, stdout);
        fputc(x, stdout);
        fputc(0, stdout);
    }

    /* BMP stores rows from bottom to top */
    for (y = height - 1; y >= 0; y--) {
        const unsigned char *row =
            pixels + (size_t)y * (size_t)width;

        if (fwrite(row, 1, (size_t)width, stdout)
            != (size_t)width)
            return 0;

        for (x = 0; x < padding; x++)
            fputc(0, stdout);
    }

    return !ferror(stdout);
}

int main(int argc, char **argv)
{
    int width = 0;
    int height = 0;
    int i;
    size_t image_size;
    unsigned char *pixels;

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    for (i = 1; i < argc; i++) {
        if ((!strcmp(argv[i], "-w") ||
             !strcmp(argv[i], "--width")) && i + 1 < argc) {
            if (!parse_positive(argv[++i], &width)) {
                fprintf(stderr, "Invalid width\n");
                return 1;
            }
        }
        else if ((!strcmp(argv[i], "-h") ||
                  !strcmp(argv[i], "--height")) && i + 1 < argc) {
            if (!parse_positive(argv[++i], &height)) {
                fprintf(stderr, "Invalid height\n");
                return 1;
            }
        }
        else {
            fprintf(stderr, "Unknown or incomplete argument: %s\n",
                    argv[i]);
            return 1;
        }
    }

    if (width == 0 || height == 0) {
        fprintf(stderr,
                "Usage: raw2bmp -w WIDTH -h HEIGHT\n");
        return 1;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        fprintf(stderr, "Image dimensions are too large\n");
        return 1;
    }

    image_size = (size_t)width * (size_t)height;

    pixels = (unsigned char *)malloc(image_size);
    if (pixels == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    if (fread(pixels, 1, image_size, stdin) != image_size) {
        fprintf(stderr, "Could not read complete RAW image\n");
        free(pixels);
        return 1;
    }

    if (fgetc(stdin) != EOF) {
        fprintf(stderr, "RAW input contains extra data\n");
        free(pixels);
        return 1;
    }

    if (!write_bmp(pixels, width, height)) {
        fprintf(stderr, "Could not write BMP image\n");
        free(pixels);
        return 1;
    }

    free(pixels);
    return 0;
}
