#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <process.h>
#define GETPID _getpid
#else
#include <unistd.h>
#define GETPID getpid
#endif

#define TXT2AN2K "C:/ASM503/NBIS/bin/txt2an2k.exe"
#define IMAGE_PATH "C:/ASM503/images/raw2an2k_input.raw"
#define FMT_PATH   "C:/ASM503/raw2an2k_input.fmt"
#define AN2K_PATH  "C:/ASM503/raw2an2k_output.an2k"

static void usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s -w width -h height [-s source] [-i id]\n"
        "Reads 8-bit grayscale RAW image from stdin and writes ANSI/NIST to stdout.\n",
        program);
}

static int write_fmt_value(FILE *fp, int record, int field,
                           const char *value)
{
    if (fprintf(fp, "%d.%d.1.1 [%d.%03d]=%s%c\n",
                record, field, record == 1 ? 1 : 14,
                field, value, 0x1f) < 0)
        return -1;

    return 0;
}

int main(int argc, char **argv)
{
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    int width = 0, height = 0, id = 1;
    const char *source = "ASM503-SOURCE";
    FILE *raw = NULL, *fmt = NULL, *out = NULL;
    unsigned char buffer[8192];
    size_t count;
    long long expected, total = 0;
    int i, status;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-w") && i + 1 < argc)
            width = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h") && i + 1 < argc)
            height = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc)
            source = argv[++i];
        else if (!strcmp(argv[i], "-i") && i + 1 < argc)
            id = atoi(argv[++i]);
        else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (width <= 0 || height <= 0 || id < 1 || id > 99) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    expected = (long long)width * height;

    raw = fopen(IMAGE_PATH, "wb");
    if (!raw) {
        perror("Cannot create temporary RAW file");
        return EXIT_FAILURE;
    }

    while ((count = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
        if (fwrite(buffer, 1, count, raw) != count) {
            perror("Cannot write RAW data");
            fclose(raw);
            // remove(IMAGE_PATH);
            return EXIT_FAILURE;
        }
        total += (long long)count;
    }

    if (ferror(stdin)) {
        fprintf(stderr, "Error reading RAW data from stdin.\n");
        fclose(raw);
        // remove(IMAGE_PATH);
        return EXIT_FAILURE;
    }

    if (fclose(raw) != 0) {
        perror("Cannot close RAW file");
        // remove(IMAGE_PATH);
        return EXIT_FAILURE;
    }

    if (total != expected) {
        fprintf(stderr,
            "RAW size mismatch: expected %lld bytes, received %lld bytes.\n",
            expected, total);
        // remove(IMAGE_PATH);
        return EXIT_FAILURE;
    }

    fmt = fopen(FMT_PATH, "wb");
    if (!fmt) {
        perror("Cannot create FMT file");
        // remove(IMAGE_PATH);
        return EXIT_FAILURE;
    }

    /* Type-1 record */
    if (write_fmt_value(fmt, 1, 1, "0") ||
        write_fmt_value(fmt, 1, 2, "0400") ||
        fprintf(fmt, "1.3.1.1 [1.003]=1%c\n", 0x1f) < 0 ||
        fprintf(fmt, "1.3.1.2 [1.003]=0%c\n", 0x1f) < 0 ||
        fprintf(fmt, "1.3.2.1 [1.003]=14%c\n", 0x1f) < 0 ||
        fprintf(fmt, "1.3.2.2 [1.003]=%02d%c\n", id, 0x1f) < 0 ||
        write_fmt_value(fmt, 1, 4, "0002") ||
        write_fmt_value(fmt, 1, 5, "20261123") ||
        write_fmt_value(fmt, 1, 6, "1") ||
        write_fmt_value(fmt, 1, 7, "000") ||
        write_fmt_value(fmt, 1, 8, "ASM503") ||
        write_fmt_value(fmt, 1, 9, "ASM503-RAW2AN2K") ||
        write_fmt_value(fmt, 1, 11, "19.69") ||
        write_fmt_value(fmt, 1, 12, "19.69")) {
        fprintf(stderr, "Error writing Type-1 fields.\n");
        fclose(fmt);
        // remove(IMAGE_PATH);
        // remove(FMT_PATH);
        return EXIT_FAILURE;
    }

    /* Type-14 record */
    if (write_fmt_value(fmt, 2, 1, "0") ||
        write_fmt_value(fmt, 2, 2, "01") ||
        write_fmt_value(fmt, 2, 3, "0") ||
        write_fmt_value(fmt, 2, 4, source) ||
        write_fmt_value(fmt, 2, 5, "20261123")) {
        fprintf(stderr, "Error writing Type-14 fields.\n");
        fclose(fmt);
        // remove(IMAGE_PATH);
        // remove(FMT_PATH);
        return EXIT_FAILURE;
    }

    {
        char value[64];

        snprintf(value, sizeof(value), "%d", width);
        if (write_fmt_value(fmt, 2, 6, value)) goto fmt_error;

        snprintf(value, sizeof(value), "%d", height);
        if (write_fmt_value(fmt, 2, 7, value)) goto fmt_error;
    }

    if (write_fmt_value(fmt, 2, 8, "2") ||
        write_fmt_value(fmt, 2, 9, "1969") ||
        write_fmt_value(fmt, 2, 10, "1969") ||
        write_fmt_value(fmt, 2, 11, "NONE") ||
        write_fmt_value(fmt, 2, 12, "8") ||
        write_fmt_value(fmt, 2, 13, "0"))
        goto fmt_error;

    if (fprintf(fmt, "2.14.1.1 [14.999]=%s%c\n", IMAGE_PATH, 0x1f) < 0)
        goto fmt_error;

    if (fclose(fmt) != 0) {
        perror("Cannot close FMT file");
        // remove(IMAGE_PATH);
        // remove(FMT_PATH);
        return EXIT_FAILURE;
    }

    status = (int)_spawnl(_P_WAIT, TXT2AN2K,
                          TXT2AN2K, FMT_PATH, AN2K_PATH,
                          (char *)NULL);
    if (status != 0) {
        fprintf(stderr, "txt2an2k failed (status=%d).\n", status);
        // remove(IMAGE_PATH);
        // remove(FMT_PATH);
        // remove(AN2K_PATH);
        return EXIT_FAILURE;
    }

    out = fopen(AN2K_PATH, "rb");
    if (!out) {
        perror("Cannot open ANSI/NIST output");
        // remove(IMAGE_PATH);
        // remove(FMT_PATH);
        // remove(AN2K_PATH);
        return EXIT_FAILURE;
    }

    while ((count = fread(buffer, 1, sizeof(buffer), out)) > 0) {
        if (fwrite(buffer, 1, count, stdout) != count) {
            perror("Cannot write ANSI/NIST to stdout");
            fclose(out);
            // remove(IMAGE_PATH);
            // remove(FMT_PATH);
            // remove(AN2K_PATH);
            return EXIT_FAILURE;
        }
    }

    if (ferror(out)) {
        fprintf(stderr, "Error reading ANSI/NIST output.\n");
        fclose(out);
        // remove(IMAGE_PATH);
        // remove(FMT_PATH);
        // remove(AN2K_PATH);
        return EXIT_FAILURE;
    }

    fclose(out);
    // remove(IMAGE_PATH);
    // remove(FMT_PATH);
    // remove(AN2K_PATH);
    return EXIT_SUCCESS;

fmt_error:
    fprintf(stderr, "Error writing Type-14 image fields.\n");
    fclose(fmt);
    // remove(IMAGE_PATH);
    // remove(FMT_PATH);
    // remove(AN2K_PATH);
    return EXIT_FAILURE;
}
