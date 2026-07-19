#include "img.hpp"
#include <iostream>
#include <stdexcept>

Img::Img() {
    // Constructor - img is automatically initialized as empty
}

Img::Img(const Img& other) {
    img = other.img.clone();
}

Img::Img(int width, int height, const cv::Scalar& fill) {
    img = cv::Mat(height, width, CV_8UC4, fill);
}

Img& Img::operator=(const Img& other) {
    if (this != &other) {
        img = other.img.clone();
    }
    return *this;
}

Img& Img::read(const std::string& path,
               const std::pair<int, int>& size,
               bool keep_aspect,
               int interpolation) {
    img = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        throw std::runtime_error("Cannot load image: " + path);
    }

    if (size.first != 0 && size.second != 0) {  // Check if size is not empty
        int target_w = size.first;
        int target_h = size.second;
        int h = img.rows;
        int w = img.cols;

        if (keep_aspect) {
            double scale = std::min(static_cast<double>(target_w) / w, 
                                   static_cast<double>(target_h) / h);
            int new_w = static_cast<int>(w * scale);
            int new_h = static_cast<int>(h * scale);
            cv::resize(img, img, cv::Size(new_w, new_h), 0, 0, interpolation);
        } else {
            cv::resize(img, img, cv::Size(target_w, target_h), 0, 0, interpolation);
        }
    }

    return *this;
}

void Img::draw_on(Img& other_img, int x, int y) const {
    if (img.empty() || other_img.img.empty()) {
        throw std::runtime_error("Both images must be loaded before drawing.");
    }

    const cv::Mat& source_img = img;
    cv::Mat& target_img = other_img.img;

    int h = source_img.rows;
    int w = source_img.cols;
    int H = target_img.rows;
    int W = target_img.cols;

    if (x < 0 || y < 0 || y + h > H || x + w > W) {
        throw std::runtime_error("Image does not fit at the specified position.");
    }

    cv::Mat roi = target_img(cv::Rect(x, y, w, h));

    if (source_img.channels() == 4) {
        std::vector<cv::Mat> src_channels; // B, G, R, A
        cv::split(source_img, src_channels);

        cv::Mat alpha, inv_alpha;
        src_channels[3].convertTo(alpha, CV_64F, 1.0 / 255.0);
        inv_alpha = 1.0 - alpha;

        std::vector<cv::Mat> roi_channels;
        cv::split(roi, roi_channels);

        const int color_channels = std::min<int>(3, static_cast<int>(roi_channels.size()));
        for (int c = 0; c < color_channels; ++c) {
            cv::Mat roi_c64, src_c64, blended;
            roi_channels[c].convertTo(roi_c64, CV_64F);
            src_channels[c].convertTo(src_c64, CV_64F);
            blended = src_c64.mul(alpha) + roi_c64.mul(inv_alpha);
            blended.convertTo(roi_channels[c], roi_channels[c].type());
        }
        cv::merge(roi_channels, roi);
    } else if (source_img.channels() == roi.channels()) {
        source_img.copyTo(roi);
    } else if (source_img.channels() == 3 && roi.channels() == 4) {
        std::vector<cv::Mat> src_channels;
        cv::split(source_img, src_channels);
        std::vector<cv::Mat> roi_channels;
        cv::split(roi, roi_channels);
        for (int c = 0; c < 3; ++c) {
            src_channels[c].copyTo(roi_channels[c]);
        }
        cv::merge(roi_channels, roi);
    } else {
        cv::Mat converted;
        cv::cvtColor(source_img, converted, cv::COLOR_BGRA2BGR);
        converted.copyTo(roi);
    }
}

void Img::put_text(const std::string& txt, int x, int y, double font_size,
                   const cv::Scalar& color, int thickness) {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    
    cv::putText(img, txt, cv::Point(x, y),
                cv::FONT_HERSHEY_SIMPLEX, font_size,
                color, thickness, cv::LINE_AA);
}

void Img::rectangle(int x, int y, int w, int h, const cv::Scalar& color, int thickness) {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    cv::rectangle(img, cv::Rect(x, y, w, h), color, thickness, cv::LINE_AA);
}

void Img::circle(int cx, int cy, int radius, const cv::Scalar& color, int thickness) {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    cv::circle(img, cv::Point(cx, cy), radius, color, thickness, cv::LINE_AA);
}

void Img::show() {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    
    cv::imshow("Image", img);
    cv::waitKey(0);
    cv::destroyAllWindows();
}

int Img::show_frame(const std::string& window_name, int delay_ms) {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }

    cv::imshow(window_name, img);
    return cv::waitKey(delay_ms);
}

void Img::on_mouse(const std::string& window_name, cv::MouseCallback callback, void* userdata) {
    cv::namedWindow(window_name, cv::WINDOW_NORMAL);
    cv::setMouseCallback(window_name, callback, userdata);
}

Img Img::clone() const {
    Img copy;
    copy.img = img.clone();
    return copy;
}