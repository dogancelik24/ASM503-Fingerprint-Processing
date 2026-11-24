#include <opencv2/opencv.hpp>
#include <opencv2/geometry/2d.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#endif

namespace {
constexpr int STEP = 16;
constexpr int BORDER = 15;
constexpr int MIN_BACKGROUND_DISTANCE = 11;
constexpr int MIN_VALID_DISTANCES = 4;
constexpr int PERIOD_MIN = 5;
constexpr int PERIOD_MAX = 20;
constexpr int MEDIAN_SIZE = 5;
constexpr int BLUR_SIZE = 3;
constexpr int FINAL_BLUR_SIZE = 33;
constexpr int DIFFUSION_SIZE = 21;

constexpr int WINDOW_WIDTH = 23;
constexpr int WINDOW_HEIGHT = 43;

cv::Mat estimate_frequency(
    const cv::Mat& input,
    const cv::Mat& mask,
    const cv::Mat& orientation)
{
    if (input.type() != CV_8U ||
        mask.type() != CV_8U ||
        orientation.type() != CV_32F)
        throw std::runtime_error("Gecersiz giris veri tipi.");

    const int width = input.cols;
    const int height = input.rows;

    cv::Mat image;
    cv::medianBlur(input, image, MEDIAN_SIZE);
    cv::GaussianBlur(
        image, image,
        cv::Size(BLUR_SIZE, BLUR_SIZE),
        0
    );

    cv::Mat padded_mask;
    cv::copyMakeBorder(
        mask,
        padded_mask,
        1, 1, 1, 1,
        cv::BORDER_CONSTANT
    );

    cv::Mat distance;
    cv::distanceTransform(
        padded_mask,
        distance,
        cv::DIST_C,
        3
    );

    distance = distance(
        cv::Rect(1, 1, width, height)
    ).clone();

    const int nw =
        ((width - BORDER) / STEP) + 1;

    const int nh =
        ((height - BORDER) / STEP) + 1;

    cv::Mat result = cv::Mat::zeros(
        nh, nw, CV_32F
    );

    const int half_width = WINDOW_WIDTH / 2;
    const int half_height = WINDOW_HEIGHT / 2;

    for (int iy = 0, y = BORDER;
         y < height - BORDER;
         ++iy, y += STEP)
    {
        for (int ix = 0, x = BORDER;
             x < width - BORDER;
             ++ix, x += STEP)
        {
            if (distance.at<float>(y, x) <
                MIN_BACKGROUND_DISTANCE)
                continue;

            const float angle =
                orientation.at<float>(y, x);

            if (!std::isfinite(angle))
                continue;

            const double rotation_angle =
                180.0 - angle * 180.0 / CV_PI;

            cv::Mat transform =
                cv::getRotationMatrix2D(
                    cv::Point2f(
                        static_cast<float>(x),
                        static_cast<float>(y)
                    ),
                    rotation_angle,
                    1.0
                );

            transform.at<double>(0, 2) +=
                half_width - x;

            transform.at<double>(1, 2) +=
                half_height - y;

            cv::Mat region;

            cv::warpAffine(
                image,
                region,
                transform,
                cv::Size(WINDOW_WIDTH, WINDOW_HEIGHT),
                cv::INTER_LINEAR,
                cv::BORDER_REFLECT
            );

            std::vector<double> signature(
                WINDOW_HEIGHT, 0.0
            );

            for (int ry = 0; ry < WINDOW_HEIGHT; ++ry) {
                const auto* row =
                    region.ptr<uint8_t>(ry);

                double sum = 0.0;

                for (int rx = 0; rx < WINDOW_WIDTH; ++rx)
                    sum += row[rx];

                signature[ry] = sum;
            }

            std::vector<int> maxima;
            std::vector<int> minima;

            for (int i = 1; i < WINDOW_HEIGHT - 1; ++i) {
                if (signature[i] > signature[i - 1] &&
                    signature[i] >= signature[i + 1])
                    maxima.push_back(i);

                if (signature[i] < signature[i - 1] &&
                    signature[i] <= signature[i + 1])
                    minima.push_back(i);
            }

            if (maxima.size() + minima.size() <
                static_cast<size_t>(2 + MIN_VALID_DISTANCES))
                continue;

            std::vector<int> valid_distances;

            auto collect_distances =
                [&](const std::vector<int>& points)
            {
                for (size_t i = 1; i < points.size(); ++i) {
                    const int d = points[i] - points[i - 1];

                    if (d >= PERIOD_MIN && d <= PERIOD_MAX)
                        valid_distances.push_back(d);
                }
            };

            collect_distances(maxima);
            collect_distances(minima);

            if (valid_distances.size() <
                static_cast<size_t>(MIN_VALID_DISTANCES))
                continue;

            std::vector<int> sorted = valid_distances;

            std::sort(sorted.begin(), sorted.end());

            double median;

            const size_t count = sorted.size();

            if (count % 2 == 0) {
                median = (
                    sorted[count / 2 - 1] +
                    sorted[count / 2]
                ) / 2.0;
            } else {
                median = sorted[count / 2];
            }

            double sum = 0.0;
            int close_count = 0;

            for (int d : valid_distances) {
                if (std::abs(d - median) <= 2.0) {
                    sum += d;
                    ++close_count;
                }
            }

            if (close_count > 0) {
                result.at<float>(iy, ix) =
                    static_cast<float>(sum / close_count);
            }
        }
    }

    const int b1 =
        BORDER - 1 - STEP / 2;

    const int low_res_border =
        (b1 + STEP - 1) / STEP + 1;

    cv::Mat padded_result;

    cv::copyMakeBorder(
        result,
        padded_result,
        low_res_border,
        low_res_border,
        low_res_border,
        low_res_border,
        cv::BORDER_CONSTANT,
        cv::Scalar(0)
    );

    cv::Mat inpaint_mask =
        padded_result == 0;

    cv::Mat filled;

    cv::inpaint(
        padded_result,
        inpaint_mask,
        filled,
        static_cast<double>(DIFFUSION_SIZE) / STEP,
        cv::INPAINT_NS
    );

    const int resized_width =
        STEP * (nw + low_res_border * 2);

    const int resized_height =
        STEP * (nh + low_res_border * 2);

    cv::Mat resized;

    cv::resize(
        filled,
        resized,
        cv::Size(resized_width, resized_height),
        0, 0,
        cv::INTER_LINEAR
    );

    const int offset =
        STEP * low_res_border - b1;

    cv::Mat final_result =
        resized(
            cv::Rect(offset, offset, width, height)
        ).clone();

    cv::GaussianBlur(
        final_result,
        final_result,
        cv::Size(FINAL_BLUR_SIZE, FINAL_BLUR_SIZE),
        0
    );

    final_result.setTo(0, mask == 0);

    return final_result;
}
}

int main(int argc, char* argv[])
{
    int width = 0;
    int height = 0;

    std::string segmentation_path;
    std::string orientation_path;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if ((arg == "-w" || arg == "--width") && i + 1 < argc)
            width = std::stoi(argv[++i]);
        else if ((arg == "-h" || arg == "--height") && i + 1 < argc)
            height = std::stoi(argv[++i]);
        else if ((arg == "-s" || arg == "--segmentation") && i + 1 < argc)
            segmentation_path = argv[++i];
        else if ((arg == "-o" || arg == "--orientation") && i + 1 < argc)
            orientation_path = argv[++i];
        else {
            std::cerr
                << "Kullanim: frequency -w GENISLIK -h YUKSEKLIK "
                << "-s MASKE.raw -o ORIENTATION.f32\n";
            return 1;
        }
    }

    if (width <= 0 || height <= 0 ||
        segmentation_path.empty() ||
        orientation_path.empty()) {
        std::cerr << "HATA: -w, -h, -s ve -o zorunludur.\n";
        return 1;
    }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    try {
        const size_t count =
            static_cast<size_t>(width) * height;

        std::vector<uint8_t> raw(count);
        std::vector<uint8_t> mask_raw(count);
        std::vector<float> orientation_raw(count);

        std::cin.read(
            reinterpret_cast<char*>(raw.data()),
            static_cast<std::streamsize>(count)
        );

        if (static_cast<size_t>(std::cin.gcount()) != count)
            throw std::runtime_error("RAW goruntu boyutu gecersiz.");

        std::ifstream mask_file(
            segmentation_path,
            std::ios::binary
        );

        if (!mask_file)
            throw std::runtime_error("Maske dosyasi acilamadi.");

        mask_file.read(
            reinterpret_cast<char*>(mask_raw.data()),
            static_cast<std::streamsize>(count)
        );

        if (static_cast<size_t>(mask_file.gcount()) != count)
            throw std::runtime_error("Maske boyutu gecersiz.");

        std::ifstream orientation_file(
            orientation_path,
            std::ios::binary
        );

        if (!orientation_file)
            throw std::runtime_error("Orientation dosyasi acilamadi.");

        orientation_file.read(
            reinterpret_cast<char*>(orientation_raw.data()),
            static_cast<std::streamsize>(count * sizeof(float))
        );

        if (static_cast<size_t>(orientation_file.gcount()) !=
            count * sizeof(float))
            throw std::runtime_error("Orientation boyutu gecersiz.");

        cv::Mat image(height, width, CV_8U, raw.data());
        cv::Mat mask(height, width, CV_8U, mask_raw.data());
        cv::Mat orientation(
            height, width, CV_32F, orientation_raw.data()
        );

        cv::Mat frequency =
            estimate_frequency(image, mask, orientation);

        std::cout.write(
            reinterpret_cast<const char*>(frequency.ptr<float>()),
            static_cast<std::streamsize>(
                frequency.total() * sizeof(float)
            )
        );

        if (!std::cout)
            throw std::runtime_error("Frekans ciktisi yazilamadi.");

        std::cerr
            << "XSFFE tamamlandi: "
            << width << "x" << height
            << '\n';

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "HATA: " << e.what() << '\n';
        return 1;
    }
}
