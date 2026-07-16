#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <filesystem>

class Img {
public:
    Img();
    Img(const Img& other);
    Img& operator=(const Img& other);

    /**
     * Load image from path and optionally resize.
     * 
     * @param path Image file to load
     * @param size Target size in pixels (width, height). If empty, keep original
     * @param keep_aspect If true, shrink so the longer side fits size while preserving aspect ratio
     * @param interpolation OpenCV interpolation flag (e.g., cv::INTER_AREA for shrink, cv::INTER_LINEAR for enlarge)
     * @return Reference to this object for method chaining
     */
    Img& read(const std::string& path,
              const std::pair<int, int>& size = {},
              bool keep_aspect = false,
              int interpolation = cv::INTER_AREA);
    
    /**
     * Draw this image onto another image at position (x, y)
     * 
     * @param other_img The target image to draw on
     * @param x X coordinate for top-left corner
     * @param y Y coordinate for top-left corner
     */
    void draw_on(Img& other_img, int x, int y) const;
    
    /**
     * Put text on the image
     * 
     * @param txt Text to draw
     * @param x X coordinate for text position
     * @param y Y coordinate for text position (baseline)
     * @param font_size Font scale factor
     * @param color Text color (BGR or BGRA)
     * @param thickness Text thickness
     */
    void put_text(const std::string& txt, int x, int y, double font_size,
                  const cv::Scalar& color = cv::Scalar(255, 255, 255, 255),
                  int thickness = 1);

    /**
     * Draw a rectangle outline (or filled, if thickness < 0) directly on this image.
     * Used for selection highlights, debug grids, etc. All on-screen drawing must
     * go through Img, so this is the one place cv::rectangle is called from.
     */
    void rectangle(int x, int y, int w, int h,
                   const cv::Scalar& color = cv::Scalar(0, 255, 255, 255),
                   int thickness = 2);

    /**
     * Display the image in a window (blocking - waits for any key, then closes the window).
     * Kept for simple one-off tests/demos.
     */
    void show();

    /**
     * Display the image in a persistent, named window without blocking or destroying
     * the window afterwards. Meant to be called once per frame inside a game loop.
     *
     * @param window_name Name of the window (created the first time it's used)
     * @param delay_ms How long to wait for a key press (cv::waitKey delay)
     * @return The key code pressed during the wait, or -1 if none
     */
    int show_frame(const std::string& window_name, int delay_ms = 1);

    /**
     * Register a mouse callback on a named window (creates the window if needed).
     * This is the only supported way to receive mouse/click events, so all input
     * handling also stays routed through Img.
     */
    static void on_mouse(const std::string& window_name, cv::MouseCallback callback, void* userdata = nullptr);

    /**
     * Returns a deep copy of this image (independent pixel buffer).
     */
    Img clone() const;

    int width() const { return img.cols; }
    int height() const { return img.rows; }

    /**
     * Get the underlying OpenCV Mat
     */
    const cv::Mat& get_mat() const { return img; }
    
    /**
     * Check if image is loaded
     */
    bool is_loaded() const { return !img.empty(); }

private:
    cv::Mat img;
};