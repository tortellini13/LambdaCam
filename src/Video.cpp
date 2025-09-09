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
#include "Structs.hpp"          // Custom strucs and enums
#include "LambdaColor.hpp"      // List of LambdaColors
#include "LambdaUtil.hpp"       // Error reporting function

//=====================================================================================

/* Class constructors and destructors */

Video::Video():
    vid_cap(0) // Initialize video capture with default camera (0)
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

bool Video::processFrame(array2D<float>& beamformed_data, const float min, const float max, const float alpha)
{
    // Resize and scale the input data
    beamformed_data_buffer.copy(beamformed_data);

    // Generate a heatmap from the input data
    // Overlay the heatmap onto the video frame
    // Draw ImGui elements
    // Display the frame on the ImGui window
    
    // Render all ImGui elements
    renderImGui(beamformed_data_buffer, min, max, alpha);

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
        screen_width = displayMode.w;
        screen_height = displayMode.h;
        std::cout << "Video: Screen size: " << screen_width << "x" << screen_height << "\n";
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
    window = SDL_CreateWindow("LambdaCam", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, window_flags);

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

    // Apply UI style
    // initUIStyle();

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
    std::cout << "Video: Requested frame size: " << frame_width << "x" << frame_height << "\n";

    // Set the properties for the video capture
    vid_cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
    vid_cap.set(cv::CAP_PROP_FRAME_WIDTH, frame_width);
    vid_cap.set(cv::CAP_PROP_FRAME_HEIGHT, frame_height);
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

bool Video::renderImGui(array2D<float>& beamformed_data, const float min, const float max, const float alpha)
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

    // Retrieve window size and calculate scaling factors
    window_size = ImGui::GetIO().DisplaySize;
    x_factor = window_size.x / 1920.0f; // Assuming unit screen size of 1920x1080
    y_factor = window_size.y / 1080.0f;
    
    // Create ImGui window at top left with constant size
    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height), ImGuiCond_Always);
    ImGui::Begin("LambdaCam", nullptr, 
                ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoCollapse | 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_MenuBar);
    
    /* Create all GUI elements */

    // Create sliders
    ImGui::SetCursorPos(windowPos(1000, 800)); // Set starting location for sliders
    slider("Slider 1", &val_1, 0.0f, 100.0f, 50, 200, slider_spacing);
    slider("Slider 2", &val_2, 0.0f, 100.0f, 50, 200, slider_spacing);
    slider("Slider 3", &val_1, 0.0f, 100.0f, 50, 200, slider_spacing);
    slider("Slider 4", &val_2, 0.0f, 100.0f, 50, 200, slider_spacing);
    slider("Slider 5", &val_1, 0.0f, 100.0f, 50, 200, slider_spacing);

    // Apply heatmap and display the frame
    applyHeatmap(beamformed_data, min, max, alpha); // Apply heatmap to the current frame
    frametoTexture();
    ImGui::SetCursorPos(windowPos(20, 20));
    ImGui::Image((ImTextureID)(intptr_t)texture_id, ImVec2(display_frame.cols, display_frame.rows));
    
    // Main menu bar
    mainMenuBar();

    // End the window
    ImGui::End(); 

    // Render UI
    ImGui::Render();
    glViewport(0, 0, screen_width, screen_height); 
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);

    return true;
} // end renderImgui

//=====================================================================================

/* Helper functions for ImGui UI elements */

void Video::initUIStyle()
{
    // Initialize style element
    ImGuiStyle& style = ImGui::GetStyle();

    // Set UI colors
    style.Colors[ImGuiCol_Text]                      = LColor::off_white;
    style.Colors[ImGuiCol_TextDisabled]              = LColor::light_grey_t;
    style.Colors[ImGuiCol_WindowBg]                  = LColor::dark_grey_t;
    style.Colors[ImGuiCol_ChildBg]                   = LColor::base;
    style.Colors[ImGuiCol_PopupBg]                   = LColor::base;
    style.Colors[ImGuiCol_Border]                    = LColor::base;
    style.Colors[ImGuiCol_BorderShadow]              = LColor::base;
    style.Colors[ImGuiCol_FrameBg]                   = LColor::base;
    style.Colors[ImGuiCol_FrameBgHovered]            = LColor::base;
    style.Colors[ImGuiCol_FrameBgActive]             = LColor::base;
    style.Colors[ImGuiCol_TitleBg]                   = LColor::base;
    style.Colors[ImGuiCol_TitleBgActive]             = LColor::base;
    style.Colors[ImGuiCol_TitleBgCollapsed]          = LColor::base;
    style.Colors[ImGuiCol_MenuBarBg]                 = LColor::base;
    style.Colors[ImGuiCol_ScrollbarBg]               = LColor::base;
    style.Colors[ImGuiCol_ScrollbarGrab]             = LColor::base;
    style.Colors[ImGuiCol_ScrollbarGrabHovered]      = LColor::base;
    style.Colors[ImGuiCol_ScrollbarGrabActive]       = LColor::base;
    style.Colors[ImGuiCol_CheckMark]                 = LColor::base;
    style.Colors[ImGuiCol_SliderGrab]                = LColor::red;
    style.Colors[ImGuiCol_SliderGrabActive]          = LColor::pink;
    style.Colors[ImGuiCol_Button]                    = LColor::base;
    style.Colors[ImGuiCol_ButtonHovered]             = LColor::base;
    style.Colors[ImGuiCol_ButtonActive]              = LColor::base;
    style.Colors[ImGuiCol_Header]                    = LColor::base;
    style.Colors[ImGuiCol_HeaderHovered]             = LColor::base;
    style.Colors[ImGuiCol_HeaderActive]              = LColor::base;
    style.Colors[ImGuiCol_Separator]                 = LColor::base;
    style.Colors[ImGuiCol_SeparatorHovered]          = LColor::base;
    style.Colors[ImGuiCol_SeparatorActive]           = LColor::base;
    style.Colors[ImGuiCol_ResizeGrip]                = LColor::base;
    style.Colors[ImGuiCol_ResizeGripHovered]         = LColor::base;
    style.Colors[ImGuiCol_ResizeGripActive]          = LColor::base;
    style.Colors[ImGuiCol_TabHovered]                = LColor::base;
    style.Colors[ImGuiCol_Tab]                       = LColor::base;
    style.Colors[ImGuiCol_TabSelected]               = LColor::base;
    style.Colors[ImGuiCol_TabSelectedOverline]       = LColor::base;
    style.Colors[ImGuiCol_TabDimmed]                 = LColor::base;
    style.Colors[ImGuiCol_TabDimmedSelected]         = LColor::base;
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = LColor::base;
    style.Colors[ImGuiCol_PlotLines]                 = LColor::base;
    style.Colors[ImGuiCol_PlotLinesHovered]          = LColor::base;
    style.Colors[ImGuiCol_PlotHistogram]             = LColor::base;
    style.Colors[ImGuiCol_PlotHistogramHovered]      = LColor::base;
    style.Colors[ImGuiCol_TableHeaderBg]             = LColor::base;
    style.Colors[ImGuiCol_TableBorderStrong]         = LColor::base;
    style.Colors[ImGuiCol_TableBorderLight]          = LColor::base;
    style.Colors[ImGuiCol_TableRowBg]                = LColor::base;
    style.Colors[ImGuiCol_TableRowBgAlt]             = LColor::base;
    style.Colors[ImGuiCol_TextLink]                  = LColor::base;
    style.Colors[ImGuiCol_TextSelectedBg]            = LColor::base;
    style.Colors[ImGuiCol_DragDropTarget]            = LColor::base;
    style.Colors[ImGuiCol_NavCursor]                 = LColor::base;
    style.Colors[ImGuiCol_NavWindowingHighlight]     = LColor::base;
    style.Colors[ImGuiCol_NavWindowingDimBg]         = LColor::base;
    style.Colors[ImGuiCol_ModalWindowDimBg]          = LColor::base;
} // end setUIStyle

ImVec2 Video::windowPos(const int x, const int y)
{
    return ImVec2(x * x_factor, y * y_factor);
} // end windowPos

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

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::MenuItem("Select Audio Device"))
                LUtil::error("Video", "Select Audio Device not implemented yet");
        
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
} // end mainMenuBar

void Video::slider(const char* label, float* value, float min, float max, uint width, uint height, int spacing)
{
    // Get the current cursor position
    float start_x = ImGui::GetCursorPosX();
    float start_y = ImGui::GetCursorPosY();

    ImGui::BeginGroup(); // Start a new group
    ImGui::Text("%s", label); // Display the label
    std::string id = "##" + std::string(label); // Add a unique identifier to the slider
    ImGui::VSliderFloat(id.c_str(), ImVec2(width, height), value, min, max); // Create a vertical slider
    ImGui::EndGroup(); // End the group

    // Move the cursor to the next slider position
    ImGui::SetCursorPos(ImVec2(start_x + spacing + width, start_y)); // Move down by the height of the slider plus spacing
} // end slider

//=====================================================================================

/* Converts OpenCV Mat to an OpenGL texture to be displayed in ImGui */

void Video::frametoTexture()
{
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, display_frame.cols, display_frame.rows, GL_RGB, GL_UNSIGNED_BYTE, display_frame.data);
    glBindTexture(GL_TEXTURE_2D, 0);
} // end frametoTexture

void Video::applyHeatmap(array2D<float>& beamformed_data, const float min, const float max, const float alpha)
{
    // Copy the current frame to the display frame
    current_frame.copyTo(display_frame); 

    // Clamp data between min and max
    for (size_t i = 0; i < beamformed_data.dim_1; i++)
    {
        for (size_t j = 0; j < beamformed_data.dim_2; j++)
        {
            if (beamformed_data.at(i, j) < min)
                beamformed_data.at(i, j) = min;
            else if (beamformed_data.at(i, j) > max)
                beamformed_data.at(i, j) = max;
        }
    }

    // Maybe start with heatmap and make everything in place****

    // Convert data to cv::Mat
    cv::Mat clamped_data(beamformed_data.dim_1, beamformed_data.dim_2, CV_32F, beamformed_data.data);

    // Normalize data to be [0 - 255]
    cv::Mat norm_data;
    cv::normalize(clamped_data, norm_data, 0, 255, cv::NORM_MINMAX);
    norm_data.convertTo(norm_data, CV_8U);

    // Ensure heatmap is the same size as the current frame
    cv::Mat resized_data;
    cv::resize(norm_data, resized_data, current_frame.size());

    // Generate a heatmap
    cv::Mat heatmap;
    cv::applyColorMap(resized_data, heatmap, cv::COLORMAP_JET); // Apply a colormap to the heatmap data

    // Overlay the heatmap onto the current frame using the desired alpha
    cv::addWeighted(display_frame, 1.0f, heatmap, alpha, 0.0f, display_frame); // Blend the heatmap with the current frame

    // Resize the display frame to the desired size
    
} // end applyHeatmap