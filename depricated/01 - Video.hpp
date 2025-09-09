#pragma once

#include <SDL2/SDL.h> // ImGui
#include <thread>     // For multithreading
#include <atomic>     // For atomic variables
#include <mutex>      // For mutex

/* Overall Flow
- Create ImGUI window
- Draw all the UI elements
- Generate heatmap
- Record a frame from the camera
- Overlay heatmap into the camera frame
- Draw frame onto ImGUI window
*/

/* ImGui Elements
- Color bar
- Sliders vertial and horizontal
- Buttons
- Drop down menus
- Text input fields
*/

class Video
{
public:
    // Constructor
    Video(std::string camera_path = "/dev/video0");

    // Destructor
    ~Video();

    // Start and stop the video capture on a separate thread
    bool startVideo();
    void stopVideo();

    // Process a frame and display it
    bool processFrame(); // Will need to pass cv::Mat& beamformed_data

    std::atomic<bool> video_running = true; // Flag to check if the video is running

private:
    // ImGui
    bool initImGui();     // Initializes SDL2 and OpenGL3
    bool renderImGui();   // Renders a frame with ImGui
    void shutdownImGui(); // Cleans up ImGui resources

    // OpenCV
    bool initOpenCV();   // Initializes OpenCV video capture
    void captureVideo(); // Continuously captures video frames
    bool getFrame();     // Retrieves a frame from the video capture

    // Helpers for ImGui
    void setColors();
    void slider(const char* label, float* value, float min, float max, uint width, uint height);
    void button();
    void dropdown();
    void textInput();

    // Size of screen
    int screen_width;
    int screen_height;
    int slider_spacing = 10; // Spacing between sliders

    // Camera parameters
    std::string camera_path; // Path to the camera device
    int frame_width = 1280;  // Default frame width
    int frame_height = 720; // Default frame height
    int frame_rate = 30;    // Default frame rate
    
    // ImGui Variables
    const char* glsl_version;     // GLSL version
    SDL_WindowFlags window_flags; // Stores SDL window flags
    SDL_Window* window;           // SDL window
    SDL_GLContext gl_context;     // GL context
    SDL_Event event;              // SDL event

    // OpenCV Variables
    cv::VideoCapture vid_cap; // For capturing video
    cv::Mat current_frame;    // Current frame from the video capture
    cv::Mat frame_buffer_1;   // Frame buffer for storing captured frames
    cv::Mat frame_buffer_2;   // Second frame buffer for double buffering
    std::thread video_thread; // Thread for capturing video frames
    std::mutex frame_mutex;   // Mutex for synchronizing access to the frame buffers

    // Temp
    float val_1 = 0;
    float val_2 = 0;
};