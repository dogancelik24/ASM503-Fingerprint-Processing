#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static void write_u16(FILE *f, uint16_t value)
{
    fputc(value & 0xFF, f);
    fputc((value >> 8) & 0xFF, f);
}

static void write_u32(FILE *f, uint32_t value)
{
    fputc(value & 0xFF, f);
    fputc((value >> 8) & 0xFF, f);
    fputc((value >> 16) & 0xFF, f);
    fputc((value >> 24) & 0xFF, f);
}

static int parse_dimension(const char *value, int *result)
{
    char *end;
    errno = 0;
    long number = strtol(value, &end, 10);

    if (errno || *value == '\0' || *end != '\0' ||
        number <= 0 || number > 2147483647L)
        return 0;

    *result = (int)number;
    return 1;
}

static void usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s --width N --height N\n"
        "       %s -w N -h N\n", program, program);
}

int main(int argc, char **argv)
{
    int width = 0, height = 0;

    for (int i = 1; i < argc; i++) {
        if ((!strcmp(argv[i], "-w") ||
             !strcmp(argv[i], "--width")) && i + 1 < argc) {
            if (!parse_dimension(argv[++i], &width)) {
                fprintf(stderr, "Invalid width.\n");
                return 1;
            }
        }
        else if ((!strcmp(argv[i], "-h") ||
                  !strcmp(argv[i], "--height")) && i + 1 < argc) {
            if (!parse_dimension(argv[++i], &height)) {
                fprintf(stderr, "Invalid height.\n");
                return 1;
            }
        }
        else {
            usage(argv[0]);
            return 1;
        }
    }

    if (width == 0 || height == 0) {
        usage(argv[0]);
        return 1;
    }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    uint64_t row_size = ((uint64_t)width + 3) & ~((uint64_t)3);
    uint64_t image_size = row_size * (uint64_t)height;
    uint64_t file_size = 14 + 40 + 1024 + image_size;
    uint64_t pixel_count = (uint64_t)width * (uint64_t)height;

    if (image_size > UINT32_MAX ||
        file_size > UINT32_MAX ||
        pixel_count > SIZE_MAX) {
        fprintf(stderr, "Image dimensions are too large.\n");
        return 1;
    }

    unsigned char *pixels = malloc((size_t)pixel_count);
    unsigned char *row = calloc((size_t)row_size, 1);

    if (!pixels || !row) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(pixels);
        free(row);
        return 1;
    }

    size_t received = fread(pixels, 1, (size_t)pixel_count, stdin);

    if (received != (size_t)pixel_count) {
        fprintf(stderr, "Input error: expected %llu bytes, received %zu.\n",
                (unsigned long long)pixel_count, received);
        free(pixels);
        free(row);
        return 1;
    }

    if (fgetc(stdin) != EOF)
        fprintf(stderr, "Warning: extra input bytes ignored.\n");

    /* BMP file header */
    fputc('B', stdout);
    fputc('M', stdout);
    write_u32(stdout, (uint32_t)file_size);
    write_u16(stdout, 0);
    write_u16(stdout, 0);
    write_u32(stdout, 1078);

    /* BITMAPINFOHEADER */
    write_u32(stdout, 40);
    write_u32(stdout, (uint32_t)width);
    write_u32(stdout, (uint32_t)height);
    write_u16(stdout, 1);
    write_u16(stdout, 8);
    write_u32(stdout, 0);
    write_u32(stdout, (uint32_t)image_size);
    write_u32(stdout, 2835);
    write_u32(stdout, 2835);
    write_u32(stdout, 256);
    write_u32(stdout, 256);

    /* Grayscale palette */
    for (int i = 0; i < 256; i++) {
        fputc(i, stdout);
        fputc(i, stdout);
        fputc(i, stdout);
        fputc(0, stdout);
    }

    /* BMP rows are stored bottom-up */
    for (int y = height - 1; y >= 0; y--) {
        memcpy(row, pixels + (size_t)y * width, (size_t)width);

        if (fwrite(row, 1, (size_t)row_size, stdout) !=
            (size_t)row_size) {
            fprintf(stderr, "Error writing BMP data.\n");
            free(pixels);
            free(row);
            return 1;
        }
    }

    free(pixels);
    free(row);

    if (fflush(stdout) != 0) {
        fprintf(stderr, "Output error.\n");
        return 1;
    }

    return 0;
}
