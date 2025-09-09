#pragma once

#include <SDL2/SDL.h>          // ImGui
#include <GL/glew.h>           // OpenGL functions
#include <opencv2/opencv.hpp>  // Contains OpenCV functions and classes
#include <thread>              // For multithreading
#include <atomic>              // For atomic variables
#include <mutex>               // For mutex
#include "Structs.hpp"         // Custom structs
#include "imgui.h"             // ImGui

class Video
{
public:
    // Constructor
    Video();

    // Destructor
    ~Video();

    // Start and stop the video capture on a separate thread
    bool startVideo();
    void stopVideo();

    // Process a frame and display it
    bool processFrame(array2D<float>& beamformed_data, const float min, const float max, const float alpha); 

    std::atomic<bool> video_running = true; // Flag to check if the video is running

private:
    // ImGui
    bool initImGui();                         // Initializes SDL2 and OpenGL3
    bool renderImGui(array2D<float>& beamformed_data, const float min, const float max, const float alpha); // Renders a frame with ImGui

    // OpenCV
    bool initOpenCV();                        // Initializes OpenCV video capture
    bool initOpenGL();                        // Initializes OpenGL
    void captureVideo(cv::VideoCapture& cap); // Continuously captures video frames

    // Helpers for OpenCV
    void applyHeatmap(array2D<float>& beamformed_data, const float min, const float max, const float alpha);   // Generates a heatmap from input data and applies it to the current frame
    void frametoTexture(); // Converts OpenCV Mat to OpenGL texture

    // Helpers for ImGui
    void initUIStyle(); // Applies custom styles to ImGui
    ImVec2 windowPos(const int x, const int y); // Calculates window position based on scaling factors
    void mainMenuBar(); // Creates the main menu bar
    void slider(const char* label, float* value, float min, float max, uint width, uint height, int spacing); // Creates a slider
    void button();      // Creates a button
    void dropdown();    // Creates a dropdown menu
    void textInput();   // Creates a text input box

    // Size of screen
    int screen_width;
    int screen_height;
    int slider_spacing = 10; // Spacing between sliders
    ImVec2 window_size;      // Size of the ImGui window
    float x_factor;          // Scalar for resizing the x dimension based on the window size
    float y_factor;          // Scalar for resizing the y dimension based on the window size

    // Camera parameters
    int frame_width = 1280;  // Default frame width
    int frame_height = 720;  // Default frame height
    int frame_rate = 30;     // Default frame rate
    
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
    cv::Mat frame_buffer_1;   // Front buffer for the combined frame
    cv::Mat frame_buffer_2;   // Back buffer for the combined frame
    std::thread video_thread; // Thread for capturing video frames
    std::mutex frame_mutex;   // Mutex for synchronizing access to the frame buffers
    GLuint texture_id;     // ID for OpenGL textures

    // Temp
    float val_1 = 0;
    float val_2 = 0;
    array2D<float> beamformed_data_buffer;
};