#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#endif

namespace {
constexpr double PI = 3.14159265358979323846;

constexpr double SIGMA_BASE = 27.0;
constexpr double SIGMA_MULTIPLIER = 43.0;
constexpr double SIGMA_SMOOTH = 1.25;
constexpr int MEDIAN_SIZE = 5;
constexpr double PERCENTILE = 19.0;

int kernel_size(double sigma)
{
    return static_cast<int>(std::ceil(3.0 * sigma)) * 2 + 1;
}

double calculate_percentile(std::vector<float>& values, double percentile)
{
    if (values.empty())
        throw std::runtime_error("Maske icinde gecerli piksel bulunamadi.");

    std::sort(values.begin(), values.end());

    const double position =
        (percentile / 100.0) * (values.size() - 1);

    const size_t lower = static_cast<size_t>(std::floor(position));
    const size_t upper = static_cast<size_t>(std::ceil(position));

    const double fraction = position - lower;

    return values[lower] * (1.0 - fraction)
         + values[upper] * fraction;
}

cv::Mat contrast_stretch(
    const cv::Mat& image,
    const cv::Mat& mask)
{
    std::vector<float> values;
    values.reserve(image.total());

    for (int y = 0; y < image.rows; ++y) {
        const auto* img = image.ptr<uint8_t>(y);
        const auto* msk = mask.ptr<uint8_t>(y);

        for (int x = 0; x < image.cols; ++x) {
            if (msk[x] != 0)
                values.push_back(static_cast<float>(img[x]));
        }
    }

    const double v1 = calculate_percentile(values, PERCENTILE);
    const double v2 = calculate_percentile(values, 100.0 - PERCENTILE);

    cv::Mat result;

    if (v2 <= v1) {
        image.convertTo(result, CV_32F);
        return result;
    }

    cv::Mat input_float;
    image.convertTo(input_float, CV_32F);

    result = (input_float - v1) * (255.0 / (v2 - v1));
    cv::max(result, 0.0, result);
    cv::min(result, 255.0, result);

    return result;
}

void compute_n_d_strengths(
    const cv::Mat& gx2,
    const cv::Mat& gy2,
    const cv::Mat& g2xy,
    double sigma,
    cv::Mat& n,
    cv::Mat& d,
    cv::Mat& strengths)
{
    const int ksize = kernel_size(sigma);
    const cv::Size size(ksize, ksize);

    cv::Mat sum_gx2;
    cv::Mat sum_gy2;

    cv::GaussianBlur(gx2, sum_gx2, size, sigma);
    cv::GaussianBlur(gy2, sum_gy2, size, sigma);
    cv::GaussianBlur(g2xy, d, size, sigma);

    n = sum_gx2 - sum_gy2;

    cv::Mat numerator;
    cv::Mat denominator;

    cv::multiply(n, n, numerator);

    cv::Mat d_squared;
    cv::multiply(d, d, d_squared);

    numerator += d_squared;
    cv::sqrt(numerator, numerator);

    denominator = sum_gx2 + sum_gy2;

    cv::Mat safe_denominator;
    cv::max(denominator, 1e-12, safe_denominator);

    cv::divide(numerator, safe_denominator, strengths);
}

cv::Mat estimate_orientation(
    const cv::Mat& image,
    const cv::Mat& mask)
{
    // GBFOE: contrast stretching
    cv::Mat enhanced = contrast_stretch(image, mask);

    // Gaussian smoothing
    const int smooth_size = kernel_size(SIGMA_SMOOTH);

    cv::GaussianBlur(
        enhanced,
        enhanced,
        cv::Size(smooth_size, smooth_size),
        SIGMA_SMOOTH
    );

    // Median filtering
    cv::medianBlur(enhanced, enhanced, MEDIAN_SIZE);

    // Gradient calculation
    cv::Mat gx;
    cv::Mat gy;

    cv::Sobel(enhanced, gx, CV_32F, 1, 0, 3);
    cv::Sobel(enhanced, gy, CV_32F, 0, 1, 3);

    // Ignore pixels outside the segmentation mask
    for (int y = 0; y < image.rows; ++y) {
        auto* px = gx.ptr<float>(y);
        auto* py = gy.ptr<float>(y);
        const auto* pm = mask.ptr<uint8_t>(y);

        for (int x = 0; x < image.cols; ++x) {
            if (pm[x] == 0) {
                px[x] = 0.0f;
                py[x] = 0.0f;
            }
        }
    }

    cv::Mat gx2;
    cv::Mat gy2;
    cv::Mat g2xy;

    cv::multiply(gx, gx, gx2);
    cv::multiply(gy, gy, gy2);
    cv::multiply(gx, gy, g2xy, -2.0);

    // First orientation-strength estimation
    cv::Mat n;
    cv::Mat d;
    cv::Mat strengths;

    compute_n_d_strengths(
        gx2, gy2, g2xy,
        SIGMA_BASE,
        n, d, strengths
    );

    double strength_sum = 0.0;
    size_t valid_count = 0;

    for (int y = 0; y < image.rows; ++y) {
        const auto* ps = strengths.ptr<float>(y);
        const auto* pm = mask.ptr<uint8_t>(y);

        for (int x = 0; x < image.cols; ++x) {
            if (pm[x] != 0) {
                strength_sum += ps[x];
                ++valid_count;
            }
        }
    }

    if (valid_count == 0)
        throw std::runtime_error("Gecerli maske pikseli bulunamadi.");

    const double average_strength =
        strength_sum / static_cast<double>(valid_count);

    const double sigma =
        SIGMA_MULTIPLIER * (1.0 - average_strength);

    if (!std::isfinite(sigma) || sigma <= 0.0)
        throw std::runtime_error("GBFOE sigma degeri gecersiz.");

    // Recalculate orientation using adaptive sigma
    compute_n_d_strengths(
        gx2, gy2, g2xy,
        sigma,
        n, d, strengths
    );

    cv::Mat phase;
    cv::phase(n, d, phase, false);

    cv::Mat orientation(image.size(), CV_32F);

    for (int y = 0; y < image.rows; ++y) {
        const auto* pp = phase.ptr<float>(y);
        const auto* pm = mask.ptr<uint8_t>(y);
        auto* po = orientation.ptr<float>(y);

        for (int x = 0; x < image.cols; ++x) {
            if (pm[x] == 0) {
                po[x] = 0.0f;
            } else {
                double angle = std::fmod(
                    (static_cast<double>(pp[x]) + PI) / 2.0,
                    PI
                );

                if (angle < 0.0)
                    angle += PI;

                po[x] = static_cast<float>(angle);
            }
        }
    }

    std::cerr
        << "GBFOE tamamlandi: "
        << image.cols << "x" << image.rows
        << ", ortalama guc: " << average_strength
        << ", sigma: " << sigma
        << '\n';

    return orientation;
}
}

int main(int argc, char* argv[])
{
    int width = 0;
    int height = 0;
    std::string segmentation_path;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if ((arg == "-w" || arg == "--width") && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if ((arg == "-h" || arg == "--height") && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        } else if ((arg == "-s" || arg == "--segmentation") && i + 1 < argc) {
            segmentation_path = argv[++i];
        } else {
            std::cerr
                << "Kullanim: orientation -w GENISLIK -h YUKSEKLIK "
                << "-s MASKE.raw\n";
            return 1;
        }
    }

    if (width <= 0 || height <= 0 || segmentation_path.empty()) {
        std::cerr
            << "HATA: -w, -h ve -s parametreleri zorunludur.\n";
        return 1;
    }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    try {
        const size_t expected_size =
            static_cast<size_t>(width) * height;

        std::vector<uint8_t> raw(expected_size);

        std::cin.read(
            reinterpret_cast<char*>(raw.data()),
            static_cast<std::streamsize>(expected_size)
        );

        if (static_cast<size_t>(std::cin.gcount()) != expected_size) {
            throw std::runtime_error(
                "RAW giris boyutu beklenen boyutla eslesmiyor."
            );
        }

        cv::Mat image(
            height, width, CV_8U, raw.data()
        );

        std::vector<uint8_t> mask_raw(expected_size);

        std::ifstream mask_file(
            segmentation_path,
            std::ios::binary
        );

        if (!mask_file) {
            throw std::runtime_error(
                "Segmentasyon maskesi acilamadi."
            );
        }

        mask_file.read(
            reinterpret_cast<char*>(mask_raw.data()),
            static_cast<std::streamsize>(expected_size)
        );

        if (static_cast<size_t>(mask_file.gcount()) != expected_size) {
            throw std::runtime_error(
                "Segmentasyon RAW boyutu beklenen boyutla eslesmiyor."
            );
        }

        cv::Mat mask(
            height, width, CV_8U, mask_raw.data()
        );

        cv::Mat orientation = estimate_orientation(image, mask);

        const size_t output_size =
            orientation.total() * sizeof(float);

        std::cout.write(
            reinterpret_cast<const char*>(orientation.ptr<float>()),
            static_cast<std::streamsize>(output_size)
        );

        if (!std::cout)
            throw std::runtime_error("YONELIM ciktisi yazilamadi.");

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "HATA: " << e.what() << '\n';
        return 1;
    }
}
