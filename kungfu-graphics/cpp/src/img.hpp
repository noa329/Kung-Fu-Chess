#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <filesystem>

class Img {
public:
    Img();
    Img(const Img& other);
    Img& operator=(const Img& other);

    /** Blank canvas of the given size, filled with a solid color (default opaque black). */
    Img(int width, int height, const cv::Scalar& fill = cv::Scalar(0, 0, 0, 255));

    Img& read(const std::string& path,
              const std::pair<int, int>& size = {},
              bool keep_aspect = false,
              int interpolation = cv::INTER_AREA);
   
    void draw_on(Img& other_img, int x, int y) const;
    
   
    void put_text(const std::string& txt, int x, int y, double font_size,
                  const cv::Scalar& color = cv::Scalar(255, 255, 255, 255),
                  int thickness = 1);

    
    void rectangle(int x, int y, int w, int h,
                   const cv::Scalar& color = cv::Scalar(0, 255, 255, 255),
                   int thickness = 2);

    
    void circle(int cx, int cy, int radius,
                const cv::Scalar& color = cv::Scalar(0, 0, 255, 255),
                int thickness = 3);

   
    void show();

    
    int show_frame(const std::string& window_name, int delay_ms = 1);

   
    static void on_mouse(const std::string& window_name, cv::MouseCallback callback, void* userdata = nullptr);

    Img clone() const;

    int width() const { return img.cols; }
    int height() const { return img.rows; }

    const cv::Mat& get_mat() const { return img; }
    
    bool is_loaded() const { return !img.empty(); }

private:
    cv::Mat img;
};