#pragma once

#include <SDL2/SDL.h>          // ImGui
#include <GL/glew.h>           // OpenGL functions
#include <opencv2/opencv.hpp>  // Contains OpenCV functions and classes
#include <thread>              // For multithreading
#include <atomic>              // For atomic variables
#include <mutex>               // For mutex
#include "Structs.hpp"         // Custom structs
#include "Widgets.hpp"         // Custom ImGui widgets
#include "imgui.h"             // ImGui
#include "Config.hpp"          // Config struct

class Video
{
public:
    // Constructor
    Video(Config& global_config);

    // Destructor
    ~Video();

    // Start and stop the video capture on a separate thread
    bool startVideo();
    void stopVideo();

    // Process a frame and display it
    bool processFrame(array2D<float>& beamformed_data); 

    std::atomic<bool> video_running = true; // Flag to check if the video is running

private:
    // ImGui
    bool initImGui();                         // Initializes SDL2 and OpenGL3
    bool renderImGui(array2D<float>& beamformed_data); // Renders a frame with ImGui

    // OpenCV
    bool initOpenCV();                        // Initializes OpenCV video capture
    bool initOpenGL();                        // Initializes OpenGL
    void captureVideo(cv::VideoCapture& cap); // Continuously captures video frames

    // Helpers for OpenCV
    void applyHeatmap(array2D<float>& beamformed_data, const float min, const float max, const float alpha, const ImVec2 camera_size); // Generates a heatmap from input data and applies it to the current frame
    void frametoTexture();    // Converts OpenCV Mat to OpenGL texture
    cv::Mat resizeAndCrop(const cv::Mat& src, const ImVec2& target_size); // Resizes and crops an image to fit the target size
    cv::Mat generateLegend(const ImVec2 size); // Generates a legend for the heatmap

    // UI Elements
    void mainMenuBar(); // Creates the main menu bar
    void frequencySlider(); // Creates frequency slider graph

    // Helpers for ImGui
    void applyUIStyle(); // Applies custom styles to ImGui
    ImVec2 windowScale(const int x, const int y); // Scales x and y values to fit in the window

    // Global configuration object
    Config &config; // Reference to the global configuration object

    // Size of screen
    ImVec2 screen_size; // Screen size (used for scaling UI elements)
    ImVec2 window_size; // Size of the ImGui window
    ImVec2 scale;       // Scale factor for resizing UI elements

    // Camera parameters
    ImVec2 frame_size{1280, 720}; // Default camera frame size
    int frame_rate = 30;          // Default frame rate
    
    // ImGui Variables
    const char* glsl_version;     // GLSL version
    SDL_WindowFlags window_flags; // Stores SDL window flags
    SDL_Window* window;           // SDL window
    SDL_GLContext gl_context;     // GL context
    SDL_Event event;              // SDL event

    // OpenCV Variables
    cv::VideoCapture vid_cap; // For capturing video
    cv::Mat current_frame;    // Current frame from the video capture
    cv::Mat display_frame;    // Frame to be displayed with heatmap overlay
    std::thread video_thread; // Thread for capturing video frames
    std::mutex frame_mutex;   // Mutex for synchronizing access to the frame buffers
    GLuint texture_id;        // ID for OpenGL textures

    // UI Elements
    LCheckBox record{"Record"};
    LButton screenshot{"Screenshot"};

    FrequencySlider freq_slider{"Frequency (Hz)", 20.0f, 20000.0f};
    LButton bump_freq_up{"+"};
    LButton bump_freq_down{"-"};

    LSlider min_slider{"Min", -100.0f, 0.0f};
    LSlider max_slider{"Max", -100.0f, 0.0f};
    LSlider alpha_slider{"Alpha", 0.0f, 1.0f};

    LCheckBox checkbox_1;
    LDropDown dropdown_1;
    LTextInput text_input_1;

    // Temp
    array2D<float> beamformed_data_buffer;
};