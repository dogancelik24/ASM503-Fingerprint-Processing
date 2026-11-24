#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct Image {
    int width = 0;
    int height = 0;
    int bitsPerPixel = 0;
    std::vector<uint8_t> pixels;
};

uint16_t readU16(std::ifstream& file) {
    uint8_t b[2];
    file.read(reinterpret_cast<char*>(b), 2);

    if (!file) {
        throw std::runtime_error("BMP basligi okunamadi.");
    }

    return static_cast<uint16_t>(b[0] | (b[1] << 8));
}

uint32_t readU32(std::ifstream& file) {
    uint8_t b[4];
    file.read(reinterpret_cast<char*>(b), 4);

    if (!file) {
        throw std::runtime_error("BMP basligi okunamadi.");
    }

    return static_cast<uint32_t>(
        b[0] |
        (static_cast<uint32_t>(b[1]) << 8) |
        (static_cast<uint32_t>(b[2]) << 16) |
        (static_cast<uint32_t>(b[3]) << 24)
    );
}

Image loadBMP(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);

    if (!file) {
        throw std::runtime_error("Dosya acilamadi: " + filename);
    }

    if (readU16(file) != 0x4D42) {
        throw std::runtime_error("Gecerli bir BMP dosyasi degil: " + filename);
    }

    readU32(file); // Dosya boyutu
    readU32(file); // Ayrılmış alanlar

    uint32_t pixelOffset = readU32(file);
    uint32_t headerSize = readU32(file);

    if (headerSize < 40) {
        throw std::runtime_error("Desteklenmeyen BMP baslik formati.");
    }

    int32_t width = static_cast<int32_t>(readU32(file));
    int32_t signedHeight = static_cast<int32_t>(readU32(file));

    uint16_t planes = readU16(file);
    uint16_t bitsPerPixel = readU16(file);
    uint32_t compression = readU32(file);

    if (width <= 0 || signedHeight == 0 ||
        signedHeight == std::numeric_limits<int32_t>::min()) {
        throw std::runtime_error("Gecersiz BMP boyutlari.");
    }

    if (planes != 1 || bitsPerPixel != 8 || compression != 0) {
        throw std::runtime_error(
            "Yalnizca sikistirilmamis 8-bit BMP destekleniyor."
        );
    }

    Image image;
    image.width = width;
    image.height = signedHeight < 0 ? -signedHeight : signedHeight;
    image.bitsPerPixel = bitsPerPixel;

    const uint64_t rowSize =
        (static_cast<uint64_t>(image.width) + 3) & ~uint64_t(3);

    const uint64_t pixelCount =
        static_cast<uint64_t>(image.width) * image.height;

    if (pixelCount > std::numeric_limits<size_t>::max() ||
        rowSize > std::numeric_limits<size_t>::max()) {
        throw std::runtime_error("Goruntu boyutu cok buyuk.");
    }

    image.pixels.resize(static_cast<size_t>(pixelCount));

    file.seekg(pixelOffset, std::ios::beg);

    if (!file) {
        throw std::runtime_error("BMP piksel verisine erisilemedi.");
    }

    std::vector<uint8_t> row(static_cast<size_t>(rowSize));

    for (int y = 0; y < image.height; ++y) {
        file.read(
            reinterpret_cast<char*>(row.data()),
            static_cast<std::streamsize>(rowSize)
        );

        if (!file) {
            throw std::runtime_error("BMP piksel verisi eksik.");
        }

        int targetY = signedHeight > 0
            ? image.height - 1 - y
            : y;

        for (int x = 0; x < image.width; ++x) {
            image.pixels[
                static_cast<size_t>(targetY) * image.width + x
            ] = row[x];
        }
    }

    return image;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr
            << "Kullanim: comp_img.exe <girdi1.bmp> <girdi2.bmp>\n";
        return 1;
    }

    try {
        Image image1 = loadBMP(argv[1]);
        Image image2 = loadBMP(argv[2]);

        if (image1.width != image2.width ||
            image1.height != image2.height) {
            throw std::runtime_error(
                "Goruntu boyutlari ayni degil."
            );
        }

        uint64_t sad = 0;
        uint8_t maxDifference = 0;

        for (size_t i = 0; i < image1.pixels.size(); ++i) {
            int difference = std::abs(
                static_cast<int>(image1.pixels[i]) -
                static_cast<int>(image2.pixels[i])
            );

            sad += static_cast<uint64_t>(difference);

            maxDifference = std::max(
                maxDifference,
                static_cast<uint8_t>(difference)
            );
        }

        std::cout << "Goruntu boyutu : "
                  << image1.width << "x" << image1.height << '\n';

        std::cout << "Piksel sayisi  : "
                  << image1.pixels.size() << '\n';

        std::cout << "SAD            : " << sad << '\n';
        std::cout << "MAX            : "
                  << static_cast<int>(maxDifference) << '\n';

    } catch (const std::exception& e) {
        std::cerr << "Hata: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
