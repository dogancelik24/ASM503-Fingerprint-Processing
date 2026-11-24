#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#endif

namespace {
constexpr int ORIENTATION_COUNT = 12;
constexpr int PERIOD_COUNT = 7;
constexpr double PERIOD_MIN = 5.0;
constexpr double PERIOD_MAX = 16.0;
constexpr double PI = 3.14159265358979323846;

bool read_file(const std::string& path, void* buffer, std::size_t size) {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        std::cerr << "Dosya acilamadi: " << path << '\n';
        return false;
    }

    file.read(static_cast<char*>(buffer),
              static_cast<std::streamsize>(size));

    if (file.gcount() != static_cast<std::streamsize>(size)) {
        std::cerr << "Dosya boyutu hatali: " << path << '\n';
        return false;
    }

    return true;
}

bool read_stdin(std::vector<unsigned char>& image) {
    std::cin.read(
        reinterpret_cast<char*>(image.data()),
        static_cast<std::streamsize>(image.size())
    );

    if (std::cin.gcount() !=
        static_cast<std::streamsize>(image.size())) {
        std::cerr << "RAW giris boyutu yetersiz.\n";
        return false;
    }

    return true;
}

int discretize_period(float period) {
    const double clipped = std::clamp(
        static_cast<double>(period),
        PERIOD_MIN,
        PERIOD_MAX
    );

    const double value =
        (clipped - PERIOD_MIN) /
        (PERIOD_MAX - PERIOD_MIN) *
        (PERIOD_COUNT - 1);

    return static_cast<int>(std::nearbyint(value));
}

int discretize_orientation(float orientation) {
    double angle = std::fmod(
        static_cast<double>(orientation),
        PI
    );

    if (angle < 0.0)
        angle += PI;

    const double value =
        (angle / PI) * ORIENTATION_COUNT;

    return static_cast<int>(
        std::nearbyint(value)
    ) % ORIENTATION_COUNT;
}

cv::Mat create_gabor_kernel(double period, double orientation) {
    const double sigma = 5.0 * period / 12.0;

    const int size =
        static_cast<int>(std::ceil(3.0 * sigma)) * 2 + 1;

    cv::Mat kernel = cv::getGaborKernel(
        cv::Size(size, size),
        sigma,
        PI / 2.0 - orientation,
        period,
        1.0,
        0.0,
        CV_32F
    );

    kernel -= cv::mean(kernel)[0];

    const double norm = std::sqrt(
        cv::sum(kernel.mul(kernel))[0]
    );

    if (norm > 0.0)
        kernel /= norm;

    return kernel;
}
}

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    int width = 0;
    int height = 0;

    std::string mask_path;
    std::string orientation_path;
    std::string period_path;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if ((arg == "-w" || arg == "--width") && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        }
        else if ((arg == "-h" || arg == "--height") && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        }
        else if ((arg == "-s" || arg == "--segmentation") && i + 1 < argc) {
            mask_path = argv[++i];
        }
        else if ((arg == "-o" || arg == "--orientation") && i + 1 < argc) {
            orientation_path = argv[++i];
        }
        else if ((arg == "-f" || arg == "--frequency") && i + 1 < argc) {
            period_path = argv[++i];
        }
        else {
            std::cerr << "Gecersiz arguman: " << arg << '\n';
            return 1;
        }
    }

    if (width <= 0 || height <= 0 ||
        mask_path.empty() ||
        orientation_path.empty() ||
        period_path.empty()) {
        std::cerr
            << "Kullanim: gabor_filter -w GENISLIK -h YUKSEKLIK "
            << "-s MASK.raw -o ORIENTATION.f32 -f PERIOD.f32\n";
        return 1;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * height;

    std::vector<unsigned char> input(pixel_count);
    std::vector<unsigned char> mask(pixel_count);

    std::vector<float> orientations(pixel_count);
    std::vector<float> periods(pixel_count);

    if (!read_stdin(input))
        return 1;

    if (!read_file(mask_path, mask.data(), pixel_count))
        return 1;

    if (!read_file(
            orientation_path,
            orientations.data(),
            pixel_count * sizeof(float)))
        return 1;

    if (!read_file(
            period_path,
            periods.data(),
            pixel_count * sizeof(float)))
        return 1;

    for (std::size_t i = 0; i < pixel_count; ++i) {
        if (!std::isfinite(orientations[i]) ||
            !std::isfinite(periods[i])) {
            std::cerr << "Haritalarda gecersiz sayisal deger var.\n";
            return 1;
        }
    }

    cv::Mat image(height, width, CV_8UC1, input.data());
    cv::Mat inverted;
    cv::bitwise_not(image, inverted);

    std::vector<cv::Mat> bank;
    bank.reserve(ORIENTATION_COUNT * PERIOD_COUNT);

    for (int p = 0; p < PERIOD_COUNT; ++p) {
        const double period =
            PERIOD_MIN +
            (PERIOD_MAX - PERIOD_MIN) *
            p / (PERIOD_COUNT - 1);

        for (int o = 0; o < ORIENTATION_COUNT; ++o) {
            const double orientation =
                PI * o / ORIENTATION_COUNT;

            bank.push_back(
                create_gabor_kernel(period, orientation)
            );
        }
    }

    std::vector<int> filter_indices(pixel_count);

    std::vector<bool> filters_needed(bank.size(), false);

    for (std::size_t i = 0; i < pixel_count; ++i) {
        const int p = discretize_period(periods[i]);
        const int o = discretize_orientation(orientations[i]);

        const int index = p * ORIENTATION_COUNT + o;

        filter_indices[i] = index;
        filters_needed[index] = true;
    }

    std::vector<cv::Mat> responses(bank.size());

    cv::Mat zero = cv::Mat::zeros(image.size(), CV_8UC1);

    for (std::size_t i = 0; i < bank.size(); ++i) {
        if (filters_needed[i]) {
            cv::filter2D(
                inverted,
                responses[i],
                -1,
                bank[i]
            );
        } else {
            responses[i] = zero;
        }
    }

    std::vector<unsigned char> output(pixel_count);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(y) * width + x;

            const int filter_index = filter_indices[index];

            output[index] =
                responses[filter_index].at<unsigned char>(y, x);
        }
    }

    std::cout.write(
        reinterpret_cast<const char*>(output.data()),
        static_cast<std::streamsize>(output.size())
    );

    if (!std::cout) {
        std::cerr << "RAW cikisi yazilamadi.\n";
        return 1;
    }

    std::cerr << "GBFEN tamamlandi: "
              << width << "x" << height
              << ", filtre sayisi: " << bank.size()
              << '\n';

    return 0;
}
