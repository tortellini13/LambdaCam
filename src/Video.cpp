// Libraries
#include <iostream>            // Standard io
#include <thread>              // For multithreading
#include <atomic>              // For atomic variables
#include <string>              // Strings
#include <SDL2/SDL.h>          // ImGui
#include <GL/glew.h>           // OpenGL functions
#include <opencv2/opencv.hpp>  // Contains OpenCV functions and classes
#include <opencv2/imgproc.hpp> // Contains OpenCV image processing functions

// Headers
#include "Video.hpp"            // Class defs
#include "imgui.h"              // ImGui
#include "imgui_impl_sdl2.h"    // ImGui
#include "imgui_impl_opengl3.h" // ImGui
#include "implot.h"             // ImPlot
#include "Structs.hpp"          // Custom strucs and enums
#include "LambdaColor.hpp"      // List of LambdaColors
#include "LambdaUtil.hpp"       // Error reporting function

//=====================================================================================

/* Class constructors and destructors */

Video::Video(Config& global_config):
    config(global_config), // Initialize reference to global config
    vid_cap(0), // Initialize video capture with default camera (0)
    checkbox_1("Checkbox 1"),
    dropdown_1("Dropdown 1", {"Option 1", "Option 2", "Option 3"}),
    text_input_1("Text Input 1")
{
    
} // end Video

Video::~Video()
{
    stopVideo(); // Ensure video capture is stopped

    std::cout << "Video: Video object destroyed and resources cleaned up\n";
} // end ~Video

//=====================================================================================

/* Public functions for starting and stopping the UI and video capture */

bool Video::startVideo()
{
    // Initialize ImGui
    if (!initImGui())
    {
        LUtil::error("Video", "Failed to initialize ImGui");
        return false;
    }

    // Initialize OpenCV
    if (!initOpenCV())
    {
        LUtil::error("Video", "Failed to initialize OpenCV");
        return false;
    }

    // Initialize OpenGL
    vid_cap >> current_frame; // Capture a frame to initialize the texture
    cv::cvtColor(current_frame, current_frame, cv::COLOR_BGR2RGB); // Convert to RGBA format for OpenGL
    if(!initOpenGL())
    {
        LUtil::error("Video", "Failed to initialize OpenGL");
        return false;
    }

    // Dispatch a thread and start capturing video from OpenCV
    video_running = true;
    video_thread = std::thread(&Video::captureVideo, this, std::ref(vid_cap)); 
    std::cout << "Video: Video capture thread started\n";

    return true;
} // end startVideo

void Video::stopVideo()
{
    // Join the thread and stop capturing video
    video_running = false;
    if (video_thread.joinable())
        video_thread.join();
    vid_cap.release(); // Release the video capture device
    cv::destroyAllWindows(); // Close all OpenCV windows

    // Cleanup ImGui resources
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    // ImPlot::DestroyContext();
 
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
 
    // Clean up OpenGL texture
    glDeleteTextures(1, &texture_id);

} // end stopVideo
 
//=====================================================================================

/* Continuously captures video from the camera on a separate thread */

void Video::captureVideo(cv::VideoCapture& cap)
{
    while (video_running)
    {
        cv::Mat frame_buffer; // Frame buffer for capturing frames
        cap >> frame_buffer;  // Read a frame from the video capture into the frame buffer
        cv::cvtColor(frame_buffer, frame_buffer, cv::COLOR_BGR2RGB); // Convert to RGB format for OpenGL

        // Check if the frame was captured successfully
        if (frame_buffer.empty()) 
        {
            LUtil::error("Video", "Frame buffer is empty");
            continue; // Restarts the loop if frame is empty
        }

        // Try to lock the mutex to safely access the frame buffers
        if (frame_mutex.try_lock())
        {
            frame_buffer.copyTo(current_frame); // Copy the captured frame to the current frame
            frame_mutex.unlock();               // Unlock the mutex
        }
    }
} // end captureVideo

//=====================================================================================

/* Generates heatmap, overlays it on the camera frame, and displays all UI elements */

bool Video::processFrame(array2D<float>& beamformed_data)
{
    // Read from config
    config.read();

    // Resize and scale the input data
    beamformed_data_buffer.copy(beamformed_data);

    // Generate a heatmap from the input data
    // Overlay the heatmap onto the video frame
    // Draw ImGui elements
    // Display the frame on the ImGui window
    
    // Render all ImGui elements
    renderImGui(beamformed_data_buffer);

    // Write to config
    config.write();

    return true;
} // end processFrame

//=====================================================================================

/* Initialization of ImGui, OpenCV, and OpenGL */

bool Video::initImGui()
{
    std::string sdl_error;
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) 
    {
        sdl_error = SDL_GetError();
        LUtil::error("Video", "SDL_Init failed (" + sdl_error + ")");
        return false;
    }

    // Get screen size from SDL
    SDL_DisplayMode displayMode;
    if (SDL_GetCurrentDisplayMode(0, &displayMode) == 0) 
    {
        screen_size.x = displayMode.w;
        screen_size.y = displayMode.h;
        std::cout << "Video: Screen size: " << screen_size.x << "x" << screen_size.y << "\n";
    } 
    else 
    {
        sdl_error = SDL_GetError();
        LUtil::error("Video", "Failed to get display mode (" + sdl_error + ")");
    }

    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    glsl_version = "#version 300 es";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("LambdaCam", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_size.x, screen_size.y, window_flags);

    if (window == nullptr)
    {
        sdl_error = SDL_GetError();
        LUtil::error("Video", "SDL_CreateWindow (" + sdl_error + ")");
        return false;
    }

    gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr)
    {
        sdl_error = SDL_GetError();
        LUtil::error("Video", "SDL_GL_CreateContext (" + sdl_error + ")");
        return false;
    }

    SDL_GL_MakeCurrent(window, gl_context);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_context))
    {
        LUtil::error("Video", "ImGui_ImplSDL2_InitForOpenGL failed");
        return false;
    }
    if(!ImGui_ImplOpenGL3_Init(glsl_version))
    {
        LUtil::error("Video", "ImGui_ImplOpenGL3_Init failed");
        return false;
    }

    SDL_GL_SetSwapInterval(1); // Enable vsync

    std::cout << "Video: ImGui initialized successfully\n";

    return true;
} // end startImGui

bool Video::initOpenCV()
{
    // Open the video capture device
    if (!vid_cap.isOpened())
    {
        LUtil::error("Video", "Failed to open video capture device");
        return false;
    }

    // Find frame size based on screen size
    std::cout << "Video: Requested frame size: " << frame_size.x << "x" << frame_size.y << "\n";

    // Set the properties for the video capture
    vid_cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
    vid_cap.set(cv::CAP_PROP_FRAME_WIDTH, frame_size.x);
    vid_cap.set(cv::CAP_PROP_FRAME_HEIGHT, frame_size.y);
    vid_cap.set(cv::CAP_PROP_FPS, frame_rate);

    std::cout << "Video: Actual frame size is: " << vid_cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x" << vid_cap.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";
    std::cout << "Video: Actual frame rate is: " << vid_cap.get(cv::CAP_PROP_FPS) << "\n";
    std::cout << "Video: Actual backend is: " << vid_cap.getBackendName() << "\n";

    // Temporary OpecCV window
    cv::namedWindow("LambdaCam");

    std::cout << "Video: OpenCV initialized successfully\n";
    return true;
} // end initOpenCV

bool Video::initOpenGL()
{

    // Bind texture to texture_id
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);   
    
    // Allocate memory for texture (will be updated every frame)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, current_frame.cols, current_frame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);


    // Texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
} // end initOpenGL

//=====================================================================================

/* Renders all UI elements in ImGui */

bool Video::renderImGui(array2D<float>& beamformed_data)
{
    // Start a new frame
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
        {
            video_running = false; // Set the flag to stop the video capture
            return false; // Exit the loop if SDL_QUIT event is received
        }
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
        {
            video_running = false; // Set the flag to stop the video capture
            return false; // Exit the loop if the window is closed
        }
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();   
    applyUIStyle(); // Apply custom UI styles

    // Retrieve window size and calculate scaling factors
    window_size = ImGui::GetIO().DisplaySize;
    scale.x = window_size.x / 1920.0f; // Assuming unit screen size of 1920x1080
    scale.y = window_size.y / 1080.0f;
    
    // Create ImGui window at top left with constant size
    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(screen_size, ImGuiCond_Always);
    ImGui::Begin("LambdaCam", nullptr, 
                ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoCollapse | 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_MenuBar);
    
    /* Create all GUI elements */

    // Right side buttons
    ImVec2 button_size = windowScale(100, 100);
    record.update(windowScale(1820, 19), button_size);
    screenshot.update(windowScale(1820, 119), button_size);

    // Frequency Slider
    freq_slider.update(windowScale(101, 985), windowScale(759, 90));
    ImVec2 freq_button_size = windowScale(90, 90);
    bump_freq_up.update(windowScale(864, 985), freq_button_size);
    bump_freq_down.update(windowScale(5, 985), freq_button_size);

    // Control Sliders
    ImVec2 slider_size = windowScale(100, 700);
    min_slider.update(windowScale(965, 355), slider_size, config.min);
    max_slider.update(windowScale(1070, 355), slider_size, config.max);
    midpoint_slider.update(windowScale(1175, 355), slider_size, config.midpoint);
    alpha_slider.update(windowScale(1280, 355), slider_size, config.alpha);





    // Create checkboxes
    checkbox_1.update(windowScale(1400, 100), button_size);

    // Create dropdowns
    ImVec2 dropdown_size = windowScale(200, 25);
    dropdown_1.update(windowScale(1600, 100), dropdown_size);

    // Create text inputs
    ImVec2 text_input_size = windowScale(200, 25); 
    text_input_1.update(windowScale(1600, 150), text_input_size);

    // Display camera feed with heatmap overlay
    ImVec2 camera_size = windowScale(960, 960);
    if (config.show_heatmap)
        applyHeatmap(beamformed_data, config.min, config.max, config.alpha, camera_size); // Apply heatmap to the current frame

    else
        display_frame = resizeAndCrop(current_frame, camera_size);   // Resize and crop the current frame
    
    frametoTexture();
    ImGui::SetCursorPos(ImVec2(0, 19)); // Cursor to place camera
    ImGui::Image((ImTextureID)(intptr_t)texture_id, ImVec2(display_frame.cols, display_frame.rows));

    // Check if heatmap is hovered and display value at cursor
    if (config.show_heatmap && ImGui::IsItemHovered())
    {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 image_pos = ImGui::GetItemRectMin();

        int x = static_cast<int>(mouse_pos.x - image_pos.x);
        int y = static_cast<int>(mouse_pos.y - image_pos.y);

        // Map from display_frame pixel coordinates → beamformed_data coordinates
        int dataX = static_cast<int>((float)x / display_frame.cols * beamformed_data.dim_1);
        int dataY = static_cast<int>((float)y / display_frame.rows * beamformed_data.dim_2);

        // Ensure within bounds of beamformed_data
        if (dataX >= 0 && dataX < static_cast<int>(beamformed_data.dim_1) &&
            dataY >= 0 && dataY < static_cast<int>(beamformed_data.dim_2))
        {
            float value = beamformed_data.at(dataY, dataX);
            ImGui::BeginTooltip();
            ImGui::Text("(%d, %d) %.2f", dataX, dataY, value);
            ImGui::EndTooltip();
        }
    }

    // Main menu bar
    mainMenuBar();

    // End the window
    ImGui::End(); 

    // Render UI
    ImGui::Render();
    glViewport(0, 0, screen_size.x, screen_size.y); 
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);

    return true;
} // end renderImgui

//=====================================================================================

/* UI Elements */

// Might get turned into a struct later***
void Video::mainMenuBar()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit"))
                video_running = false;

            if (ImGui::MenuItem("New Measurement"))
                LUtil::error("Video", "New Measurement not implemented yet");

            if (ImGui::MenuItem("Save Measurement"))
                LUtil::error("Video", "Save Measurement not implemented yet");

            if (ImGui::MenuItem("Open Measurement"))
                LUtil::error("Video", "Open Measurement not implemented yet");

            if (ImGui::MenuItem("Select Directory"))
                LUtil::error("Video", "Select Directory not implemented yet");

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::MenuItem("Select Audio Device"))
                LUtil::error("Video", "Select Audio Device not implemented yet");

            if (ImGui::MenuItem("Select Camera Device"))
                LUtil::error("Video", "Select Camera Device not implemented yet");
        
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Display"))
        {
            if (ImGui::MenuItem("Show Heatmap", nullptr, &config.show_heatmap)) {}
            
            if (ImGui::MenuItem("Show Heatmap Legend", nullptr, &config.show_heatmap_legend))
                LUtil::error("Video", "Show Heatmap Legend not implemented yet");

            if (ImGui::MenuItem("Show Max Cursor", nullptr, &config.show_max_cursor))
                LUtil::error("Video", "Show Max Cursor not implemented yet");  
                
            if (ImGui::MenuItem("Light/Dark Mode", nullptr, &config.dark_mode))
                LUtil::error("Video", "Light/Dark Mode not implemented yet");

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Recording"))
        {
            if (ImGui::MenuItem("Record Data"))
                LUtil::error("Video", "Record Data not implemented yet");

            if (ImGui::MenuItem("Record Screen"))
                LUtil::error("Video", "Record Video not implemented yet");
            
            if (ImGui::MenuItem("Record Audio"))
                LUtil::error("Video", "Record Audio not implemented yet");

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
} // end mainMenuBar

//=====================================================================================

/* Helper functions for ImGui */

void Video::applyUIStyle()
{
    // Initialize style element
    ImGuiStyle& style = ImGui::GetStyle();

    // Set UI element styles
    style.WindowRounding    = 6.0f;  // Rounds window corners
    style.ChildRounding     = 6.0f;  // Rounds child windows
    style.FrameRounding     = 7.0f;  // Rounds buttons, sliders, inputs
    style.PopupRounding     = 6.0f;  // Rounds popup windows
    style.ScrollbarRounding = 12.0f; // Rounds scrollbar grab
    style.GrabRounding      = 4.0f;  // Rounds slider/button grabs
    style.TabRounding       = 4.0f;  // Rounds tabs

    // Set UI colors
    style.Colors[ImGuiCol_Text]                      = LColor::off_white;     // Main text
    style.Colors[ImGuiCol_TextDisabled]              = LColor::light_grey_t;  // Disabled text
    style.Colors[ImGuiCol_WindowBg]                  = LColor::dark_grey_t;   // Background of main window
    style.Colors[ImGuiCol_ChildBg]                   = LColor::medium_grey;   // Panels, child windows
    style.Colors[ImGuiCol_PopupBg]                   = LColor::medium_grey;   // Popups / dropdowns
    style.Colors[ImGuiCol_Border]                    = LColor::black;         // Subtle borders
    style.Colors[ImGuiCol_BorderShadow]              = LColor::black;         // Shadow effect
    style.Colors[ImGuiCol_FrameBg]                   = LColor::medium_grey;   // Input boxes, sliders background
    style.Colors[ImGuiCol_FrameBgHovered]            = LColor::sky_blue;      // Hover effect for inputs
    style.Colors[ImGuiCol_FrameBgActive]             = LColor::red;           // Active input / slider grab
    style.Colors[ImGuiCol_TitleBg]                   = LColor::black;         // Window title background
    style.Colors[ImGuiCol_TitleBgActive]             = LColor::sky_blue;      // Active title background
    style.Colors[ImGuiCol_TitleBgCollapsed]          = LColor::medium_grey;   // Collapsed window
    style.Colors[ImGuiCol_MenuBarBg]                 = LColor::medium_grey;   // Menu bars
    style.Colors[ImGuiCol_ScrollbarBg]               = LColor::medium_grey;   // Scroll background
    style.Colors[ImGuiCol_ScrollbarGrab]             = LColor::off_white;     // Scrollbar handle
    style.Colors[ImGuiCol_ScrollbarGrabHovered]      = LColor::sky_blue;      // Hovered scrollbar handle
    style.Colors[ImGuiCol_ScrollbarGrabActive]       = LColor::red;           // Active scrollbar handle
    style.Colors[ImGuiCol_CheckMark]                 = LColor::red;           // Checkboxes
    style.Colors[ImGuiCol_SliderGrab]                = LColor::red;           // Slider handle
    style.Colors[ImGuiCol_SliderGrabActive]          = LColor::pink;          // Active slider handle
    style.Colors[ImGuiCol_Button]                    = LColor::medium_grey;   // Default button
    style.Colors[ImGuiCol_ButtonHovered]             = LColor::sky_blue;      // Hovered button
    style.Colors[ImGuiCol_ButtonActive]              = LColor::red;           // Pressed button
    style.Colors[ImGuiCol_Header]                    = LColor::medium_grey;   // Collapsing headers
    style.Colors[ImGuiCol_HeaderHovered]             = LColor::sky_blue;      // Hovered header
    style.Colors[ImGuiCol_HeaderActive]              = LColor::red;           // Active header
    style.Colors[ImGuiCol_Separator]                 = LColor::black;         // Separators
    style.Colors[ImGuiCol_SeparatorHovered]          = LColor::sky_blue;      // Hovered separator
    style.Colors[ImGuiCol_SeparatorActive]           = LColor::red;           // Active separator
    style.Colors[ImGuiCol_ResizeGrip]                = LColor::off_white;     // Grip handle
    style.Colors[ImGuiCol_ResizeGripHovered]         = LColor::sky_blue;      // Hovered grip
    style.Colors[ImGuiCol_ResizeGripActive]          = LColor::red;           // Active grip
    style.Colors[ImGuiCol_Tab]                       = LColor::medium_grey;   // Tabs
    style.Colors[ImGuiCol_TabHovered]                = LColor::sky_blue;      // Hovered tab
    style.Colors[ImGuiCol_TabActive]                 = LColor::red;           // Active tab
    style.Colors[ImGuiCol_TabSelected]               = LColor::pink;          // Selected tab
    style.Colors[ImGuiCol_TabSelectedOverline]       = LColor::sky_blue;      // Small visual indicator
    style.Colors[ImGuiCol_TabDimmed]                 = LColor::light_grey_t;  // Dimmed tab
    style.Colors[ImGuiCol_TabDimmedSelected]         = LColor::sky_blue;      // Selected dimmed tab
    style.Colors[ImGuiCol_PlotLines]                 = LColor::off_white;     // Line plots
    style.Colors[ImGuiCol_PlotLinesHovered]          = LColor::sky_blue;
    style.Colors[ImGuiCol_PlotHistogram]             = LColor::off_white;     // Histogram
    style.Colors[ImGuiCol_PlotHistogramHovered]      = LColor::sky_blue;
    style.Colors[ImGuiCol_TextSelectedBg]            = LColor::sky_blue;      // Selected text background
    style.Colors[ImGuiCol_DragDropTarget]            = LColor::sky_blue;      // Drag target highlight
    style.Colors[ImGuiCol_NavWindowingHighlight]     = LColor::sky_blue;      // Navigation highlight
    style.Colors[ImGuiCol_NavWindowingDimBg]         = LColor::dark_grey_t;   // Dim background
    style.Colors[ImGuiCol_ModalWindowDimBg]          = LColor::dark_grey_t;   // Dim modal background
} // end setUIStyle

ImVec2 Video::windowScale(const int x, const int y)
{
    return ImVec2(x * scale.x, y * scale.y);
} // end windowPos

//=====================================================================================

/* Helper functions for OpenCV */

void Video::frametoTexture()
{
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Ensure correct byte alignment
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload the frame
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, display_frame.cols, display_frame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, display_frame.data);

    glBindTexture(GL_TEXTURE_2D, 0);
} // end frametoTexture

void Video::applyHeatmap(array2D<float>& beamformed_data, const float min, const float max, const float alpha, const ImVec2 camera_size)
{
    // Clamp data between min and max and convert to OpenCV Mat
    cv::Mat clamped_data(beamformed_data.dim_1, beamformed_data.dim_2, CV_32FC1);
    for (size_t i = 0; i < beamformed_data.dim_1; i++)
    {
        for (size_t j = 0; j < beamformed_data.dim_2; j++)
        {
            float sample = beamformed_data.at(i, j);
            if (sample < min)
                clamped_data.at<float>(i, j) = min;
            else if (sample > max)
                clamped_data.at<float>(i, j) = max;
            else
                clamped_data.at<float>(i, j) = sample;
        }
    }

    // Normalize data to be [0 - 255]
    cv::Mat norm_data;
    cv::normalize(clamped_data, norm_data, 0, 255, cv::NORM_MINMAX);
    norm_data.convertTo(norm_data, CV_8U);

    // Shift the midpoint of the heatmap for display purposes
    cv::Mat midpoint_data(norm_data.rows, norm_data.cols, CV_8U);
    float midpoint_value = midpoint_slider.value * 255.0f;
    for (int y = 0; y < norm_data.rows; y++)
    {
        for (int x = 0; x < norm_data.cols; x++)
        {
            int val = norm_data.at<uchar>(y, x);

            // Scale 0–midpoint to 0–127
            if (val <= midpoint_value)
                midpoint_data.at<uchar>(y, x) = 127.0f * (val / midpoint_value);

            // Scale midpoint–255 to 127–255
            else
                midpoint_data.at<uchar>(y, x) = 127.0f + 128.0f * ((val - midpoint_value) / (255.0f - midpoint_value));
        }
    }

    // Ensure heatmap is the same size as the current frame
    cv::Mat resized_data;
    cv::resize(midpoint_data, resized_data, current_frame.size());

    // Generate a heatmap
    cv::Mat heatmap;
    cv::applyColorMap(255 - resized_data, heatmap, cv::COLORMAP_JET); // Apply a colormap to the heatmap data

    // Overlay the heatmap onto the current frame using the desired alpha
    cv::Mat merged_frame;
    cv::addWeighted(current_frame, 1.0f, heatmap, alpha, 0.0f, merged_frame); // Blend the heatmap with the current frame

    // Resize the display frame to the desired size
    display_frame = resizeAndCrop(merged_frame, camera_size);
} // end applyHeatmap

cv::Mat Video::resizeAndCrop(const cv::Mat& src, const ImVec2& target_size)
{
    int tgt_w = std::max(1, static_cast<int>(target_size.x));
    int tgt_h = std::max(1, static_cast<int>(target_size.y));

    float src_aspect = static_cast<float>(src.cols) / src.rows;
    float tgt_aspect = static_cast<float>(tgt_w) / tgt_h;

    cv::Mat resized;

    // Compute scale to fit one dimension
    float scale;
    if (src_aspect > tgt_aspect)
        scale = static_cast<float>(tgt_h) / src.rows; // fit height
    else
        scale = static_cast<float>(tgt_w) / src.cols; // fit width

    int new_w = std::max(1, static_cast<int>(src.cols * scale));
    int new_h = std::max(1, static_cast<int>(src.rows * scale));

    cv::resize(src, resized, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR);

    // Only crop if resized image is bigger than target
    int x_offset = std::max(0, (new_w - tgt_w) / 2);
    int y_offset = std::max(0, (new_h - tgt_h) / 2);
    int crop_w   = std::min(tgt_w, resized.cols - x_offset);
    int crop_h   = std::min(tgt_h, resized.rows - y_offset);

    cv::Rect roi(x_offset, y_offset, crop_w, crop_h);

    return resized(roi).clone();
} // end resizeAndCrop

cv::Mat Video::generateLegend(const ImVec2 size)
{
    // Create a 256 x 1 gradient
    cv::Mat gradient(256, 1, CV_8U);

    for (int i = 0; i < gradient.rows; i++)
        gradient.at<uchar>(i, 0) = static_cast<uchar>(i * 255 / (gradient.rows - 1));

    // Apply colormap to the gradient
    cv::Mat color_legend;
    cv::applyColorMap(gradient, color_legend, cv::COLORMAP_JET);

    // Resize the legend
    cv::Mat resized_legend;
    cv::resize(color_legend, resized_legend, cv::Size(size.x, size.y), 0, 0, cv::INTER_LINEAR);

    return resized_legend;
} // end generateLegend