#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#endif

namespace {
constexpr double SIGMA = 4.333333333333333;
constexpr double PERCENTILE = 95.0;
constexpr double THRESHOLD = 0.2;
constexpr int CLOSING_COUNT = 6;
constexpr int OPENING_COUNT = 12;

bool read_input(std::vector<unsigned char>& data) {
    std::cin.read(
        reinterpret_cast<char*>(data.data()),
        static_cast<std::streamsize>(data.size())
    );

    return std::cin.gcount() ==
           static_cast<std::streamsize>(data.size());
}

double calculate_percentile(const cv::Mat& magnitude) {
    std::vector<float> values;
    values.reserve(magnitude.total());

    for (int y = 0; y < magnitude.rows; ++y) {
        const float* row = magnitude.ptr<float>(y);

        for (int x = 0; x < magnitude.cols; ++x)
            values.push_back(row[x]);
    }

    std::sort(values.begin(), values.end());

    const double position =
        (PERCENTILE / 100.0) * (values.size() - 1);

    const std::size_t lower =
        static_cast<std::size_t>(std::floor(position));

    const std::size_t upper =
        static_cast<std::size_t>(std::ceil(position));

    const double fraction = position - lower;

    return values[lower] * (1.0 - fraction) +
           values[upper] * fraction;
}

void keep_largest_component(cv::Mat& mask) {
    cv::Mat labels, stats, centroids;

    const int count = cv::connectedComponentsWithStats(
        mask, labels, stats, centroids, 8, CV_32S
    );

    if (count <= 1)
        return;

    int largest = 1;

    for (int i = 2; i < count; ++i) {
        if (stats.at<int>(i, cv::CC_STAT_AREA) >
            stats.at<int>(largest, cv::CC_STAT_AREA)) {
            largest = i;
        }
    }

    for (int y = 0; y < mask.rows; ++y) {
        unsigned char* mask_row = mask.ptr<unsigned char>(y);
        const int* label_row = labels.ptr<int>(y);

        for (int x = 0; x < mask.cols; ++x) {
            if (label_row[x] != largest)
                mask_row[x] = 0;
        }
    }
}

void fill_internal_holes(cv::Mat& mask) {
    cv::Mat inverse;
    cv::bitwise_not(mask, inverse);

    cv::Mat labels, stats, centroids;

    const int count = cv::connectedComponentsWithStats(
        inverse, labels, stats, centroids, 8, CV_32S
    );

    std::vector<bool> holes(count, false);

    const int width = mask.cols;
    const int height = mask.rows;

    for (int i = 0; i < count; ++i) {
        const int left = stats.at<int>(i, cv::CC_STAT_LEFT);
        const int top = stats.at<int>(i, cv::CC_STAT_TOP);
        const int component_width =
            stats.at<int>(i, cv::CC_STAT_WIDTH);
        const int component_height =
            stats.at<int>(i, cv::CC_STAT_HEIGHT);

        if (left > 0 &&
            left + component_width < width - 1 &&
            top > 0 &&
            top + component_height < height - 1) {
            holes[i] = true;
        }
    }

    for (int y = 0; y < height; ++y) {
        unsigned char* mask_row = mask.ptr<unsigned char>(y);
        const int* label_row = labels.ptr<int>(y);

        for (int x = 0; x < width; ++x) {
            if (holes[label_row[x]])
                mask_row[x] = 255;
        }
    }
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    int width = 0;
    int height = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if ((arg == "-w" || arg == "--width") && i + 1 < argc)
            width = std::stoi(argv[++i]);
        else if ((arg == "-h" || arg == "--height") && i + 1 < argc)
            height = std::stoi(argv[++i]);
        else {
            std::cerr << "Gecersiz arguman: " << arg << '\n';
            return 1;
        }
    }

    if (width <= 0 || height <= 0) {
        std::cerr
            << "Kullanim: segmentation -w GENISLIK -h YUKSEKLIK\n";
        return 1;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * height;

    std::vector<unsigned char> input(pixel_count);

    if (!read_input(input)) {
        std::cerr << "RAW giris boyutu yetersiz.\n";
        return 1;
    }

    cv::Mat image(height, width, CV_8UC1, input.data());

    // 1. Gradyan hesaplama
    cv::Mat gx, gy;
    cv::spatialGradient(image, gx, gy);

    cv::Mat gx_float, gy_float, magnitude;

    gx.convertTo(gx_float, CV_32F);
    gy.convertTo(gy_float, CV_32F);

    cv::magnitude(gx_float, gy_float, magnitude);

    // 2. Gaussian ortalama gradyan
    const int gaussian_size =
        static_cast<int>(std::ceil(3.0 * SIGMA)) * 2 + 1;

    cv::Mat averaged;
    cv::GaussianBlur(
        magnitude,
        averaged,
        cv::Size(gaussian_size, gaussian_size),
        SIGMA
    );

    // 3. Yüzdelik değere dayalı eşik
    const double norm_threshold =
        calculate_percentile(magnitude) * THRESHOLD;

    cv::Mat mask;
    cv::threshold(
        averaged,
        mask,
        norm_threshold,
        255,
        cv::THRESH_BINARY
    );

    mask.convertTo(mask, CV_8U);

    // 4. Morfolojik kapama
    const cv::Mat element = cv::getStructuringElement(
        cv::MORPH_ELLIPSE,
        cv::Size(3, 3)
    );

    cv::morphologyEx(
        mask,
        mask,
        cv::MORPH_CLOSE,
        element,
        cv::Point(-1, -1),
        CLOSING_COUNT
    );

    // 5. En büyük bağlı bileşen
    keep_largest_component(mask);

    // 6. İç boşlukları doldur
    fill_internal_holes(mask);

    // 7. Morfolojik açma
    cv::morphologyEx(
        mask,
        mask,
        cv::MORPH_OPEN,
        element,
        cv::Point(-1, -1),
        OPENING_COUNT,
        cv::BORDER_CONSTANT,
        cv::Scalar(0)
    );

    // 8. Yeniden en büyük bileşeni tut
    keep_largest_component(mask);

    // RAW maske çıktısı
    std::cout.write(
        reinterpret_cast<const char*>(mask.data),
        static_cast<std::streamsize>(pixel_count)
    );

    if (!std::cout) {
        std::cerr << "Maske cikisi yazilamadi.\n";
        return 1;
    }

    const int foreground = cv::countNonZero(mask);

    std::cerr
        << "GMFS tamamlandi: " << width << "x" << height
        << ", esik: " << norm_threshold
        << ", parmak pikseli: " << foreground
        << '\n';

    return 0;
}
