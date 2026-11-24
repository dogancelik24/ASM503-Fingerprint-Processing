#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <tiffio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static tmsize_t read_stdin(thandle_t handle, void *buffer, tmsize_t size)
{
    FILE *input = (FILE *)handle;
    return (tmsize_t)fread(buffer, 1, (size_t)size, input);
}

static tmsize_t write_disabled(thandle_t handle, void *buffer, tmsize_t size)
{
    (void)handle;
    (void)buffer;
    (void)size;
    return 0;
}

static toff_t seek_stdin(thandle_t handle, toff_t offset, int whence)
{
    FILE *input = (FILE *)handle;

    if (fseek(input, (long)offset, whence) != 0)
        return (toff_t)-1;

    long position = ftell(input);
    return position < 0 ? (toff_t)-1 : (toff_t)position;
}

static int close_stdin(thandle_t handle)
{
    (void)handle;
    return 0;
}

static toff_t size_stdin(thandle_t handle)
{
    FILE *input = (FILE *)handle;
    long current = ftell(input);

    if (current < 0 || fseek(input, 0, SEEK_END) != 0)
        return 0;

    long size = ftell(input);
    fseek(input, current, SEEK_SET);

    return size < 0 ? 0 : (toff_t)size;
}

static int map_disabled(thandle_t handle, void **base, toff_t *size)
{
    (void)handle;
    (void)base;
    (void)size;
    return 0;
}

static void unmap_disabled(thandle_t handle, void *base, toff_t size)
{
    (void)handle;
    (void)base;
    (void)size;
}

int main(void)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    TIFF *tif = TIFFClientOpen(
        "stdin", "r", (thandle_t)stdin,
        read_stdin, write_disabled, seek_stdin,
        close_stdin, size_stdin,
        map_disabled, unmap_disabled
    );

    if (!tif) {
        fprintf(stderr, "Error: TIFF input could not be opened.\n");
        return 1;
    }

    uint32_t width = 0, height = 0;
    uint16_t bits = 0, samples = 0;
    uint16_t photometric = 0, planar = 0, sample_format = 0;

    if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width) ||
        !TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height)) {
        fprintf(stderr, "Error: TIFF dimensions are missing.\n");
        TIFFClose(tif);
        return 1;
    }

    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samples);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sample_format);

    if (width == 0 || height == 0 ||
        bits != 8 || samples != 1 ||
        planar != PLANARCONFIG_CONTIG ||
        (photometric != PHOTOMETRIC_MINISBLACK &&
         photometric != PHOTOMETRIC_MINISWHITE) ||
        sample_format != SAMPLEFORMAT_UINT) {
        fprintf(stderr,
            "Error: only 8-bit, single-channel grayscale TIFF is supported.\n");
        TIFFClose(tif);
        return 1;
    }

    tmsize_t scanline_size = TIFFScanlineSize(tif);

    if (scanline_size < (tmsize_t)width) {
        fprintf(stderr, "Error: invalid TIFF scanline size.\n");
        TIFFClose(tif);
        return 1;
    }

    unsigned char *row = (unsigned char *)_TIFFmalloc(scanline_size);

    if (!row) {
        fprintf(stderr, "Error: memory allocation failed.\n");
        TIFFClose(tif);
        return 1;
    }

    for (uint32_t y = 0; y < height; y++) {
        if (TIFFReadScanline(tif, row, y, 0) < 0) {
            fprintf(stderr, "Error: cannot read TIFF row %u.\n", y);
            _TIFFfree(row);
            TIFFClose(tif);
            return 1;
        }

        if (photometric == PHOTOMETRIC_MINISWHITE) {
            for (uint32_t x = 0; x < width; x++)
                row[x] = (unsigned char)(255 - row[x]);
        }

        if (fwrite(row, 1, width, stdout) != width) {
            fprintf(stderr, "Error: cannot write RAW output.\n");
            _TIFFfree(row);
            TIFFClose(tif);
            return 1;
        }
    }

    _TIFFfree(row);
    TIFFClose(tif);

    if (fflush(stdout) != 0) {
        fprintf(stderr, "Error: output flush failed.\n");
        return 1;
    }

    return 0;
}
