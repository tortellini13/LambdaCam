// Libraries
#include <iostream>
#include <thread>              // For multithreading
#include <atomic>              // For atomic variables
#include <SDL2/SDL.h>          // ImGui
#include <GL/glew.h>           // OpenGL functions
#include <opencv2/opencv.hpp>  // Contains OpenCV functions and classes
#include <opencv2/imgproc.hpp> // Contains OpenCV image processing functions

// Headers
#include "Video.hpp"            // Class defs
#include "imgui.h"              // ImGui
#include "imgui_impl_sdl2.h"    // ImGui
#include "imgui_impl_opengl3.h" // ImGui

//=====================================================================================

Video::Video(std::string camera_path):
    camera_path(camera_path), // Initialize camera path
    vid_cap(camera_path, cv::CAP_V4L2) // Initialize video capture with V4L2 backend
{
    
} // end Video

Video::~Video()
{
    stopVideo(); // Ensure video capture is stopped
    shutdownImGui(); // Clean up ImGui resources

    std::cout << "Video object destroyed and resources cleaned up\n";
} // end ~Video

//=====================================================================================

bool Video::startVideo()
{
    // Initialize ImGui
    if (!initImGui())
    {
        std::cerr << "Error: Failed to initialize ImGui\n";
        return false;
    }

    // Initialize OpenCV
    if (!initOpenCV())
    {
        std::cerr << "Error: Failed to initialize OpenCV\n";
        return false;
    }

    // Dispatch a thread and start capturing video from OpenCV
    video_running = true;
    video_thread = std::thread(&Video::captureVideo, this); 
    std::cout << "Video capture thread started\n";

    return true;
} // end startVideo

void Video::stopVideo()
{
    // Join the thread and stop capturing video
    video_running = false;
    if (video_thread.joinable())
        video_thread.join();
} // end stopVideo

//=====================================================================================

void Video::captureVideo()
{
    while (video_running)
    {
        cv::Mat temp; // Create a temporary frame to hold the captured video frame
        if (vid_cap.read(temp) && !temp.empty())
        {
            temp.copyTo(frame_buffer_1);
            std::swap(frame_buffer_1, frame_buffer_2);
            std::cout << "temp size: " << temp.rows << "x" << temp.cols << "\n";
        }
        else
        {
            std::cerr << "Capture failed or frame is empty\n";
        }
    }

    // // cv::Mat frame;
    // while (video_running)
    // {
    //     // Read a frame from the video capture into ring buffer
    //     if (vid_cap.read(frame_buffer_1)) 
    //     {
    //         std::swap(frame_buffer_1, frame_buffer_2); // Swap buffers
    //     }
    //     else
    //     {
    //         std::cerr << "Error: Could not read frame from video capture\n";
    //         video_running = false;
    //     }
    // }
} // end captureVideo

bool Video::getFrame()
{
    if (video_running)
    {
        std::cout << "current_frame size: " << current_frame.rows << "x" << current_frame.cols << "\n";
        std::cout << "frame_buffer_1 size: " << frame_buffer_1.rows << "x" << frame_buffer_1.cols << "\n";
        std::cout << "frame_buffer_2 size: " << frame_buffer_2.rows << "x" << frame_buffer_2.cols << "\n";
        frame_buffer_2.copyTo(current_frame);
        return true;
    }

    return false;




    // // Swap the most recent frame from the buffer to the current frame
    // if (!frame_buffer_2.empty())
    // {
    //     frame_buffer_2.copyTo(current_frame);
    //     // std::swap(current_frame, frame_buffer_2); 
    //     return true;
    // }
    // else
    // {
    //     std::cerr << "Error: Frame buffer is empty\n";
    //     return false;
    // }
    // return true;
} // end getFrame

//=====================================================================================

bool Video::processFrame()
{
    // Generate a heatmap from the input data
    // Overlay the heatmap onto the video frame
    // Draw ImGui elements
    // Display the frame on the ImGui window
    
    // Retrieve a frame from the video capture
    if(!getFrame())
    {
        std::cerr << "Error: Failed to retrieve frame from video capture\n";
        return false;
    }

    cv::imshow("LambdaCam", current_frame); // Display the current frame in a window


    renderImGui();

    return true;
} // end processFrame

//=====================================================================================

bool Video::initImGui()
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) 
    {
        std::cerr << "Error: SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Get screen size from SDL
    SDL_DisplayMode displayMode;
    if (SDL_GetCurrentDisplayMode(0, &displayMode) == 0) 
    {
        screen_width = displayMode.w;
        screen_height = displayMode.h;
        std::cout << "Screen size: " << screen_width << "x" << screen_height << "\n";
    } 
    else 
    {
        std::cerr << "Failed to get display mode: " << SDL_GetError() << "\n";
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
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return false;
    }

    gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr)
    {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
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
        std::cerr << "Error: ImGui_ImplSDL2_InitForOpenGL failed\n";
        return false;
    }
    if(!ImGui_ImplOpenGL3_Init(glsl_version))
    {
        std::cerr << "Error: ImGui_ImplOpenGL3_Init failed\n";
        return false;
    }

    std::cout << "ImGui initialized successfully\n";

    return true;
} // end startImGui

bool Video::initOpenCV()
{
    // Open the video capture device
    if (!vid_cap.isOpened())
    {
        std::cerr << "Error: Could not open video capture device\n";
        return false;
    }

    // Find frame size based on screen size
    std::cout << "Requested frame size: " << frame_width << "x" << frame_height << "\n";

    // Set the desired properties for the video capture
    vid_cap.open(camera_path, cv::CAP_V4L2);
    vid_cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
    vid_cap.set(cv::CAP_PROP_FRAME_WIDTH, frame_width);
    vid_cap.set(cv::CAP_PROP_FRAME_HEIGHT, frame_height);
    vid_cap.set(cv::CAP_PROP_FPS, frame_rate);

    std::cout << "Actual frame size is: " << vid_cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x" << vid_cap.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";
    std::cout << "Actual frame rate is: " << vid_cap.get(cv::CAP_PROP_FPS) << "\n";
    std::cout << "Actual backend is: " << vid_cap.getBackendName() << "\n";

    std::cout << "OpenCV initialized successfully\n";
    return true;
} // end initOpenCV

//=====================================================================================

bool Video::renderImGui()
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
    
    // Create ImGui window at top left with constant size
    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height), ImGuiCond_Always);
    ImGui::Begin("LambdaCam", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    
    // Create sliders
    ImGui::SetCursorPos(ImVec2(100, 100)); // Set starting location for sliders
    slider("Slider 1", &val_1, 0.0f, 100.0f, 50, 200);
    slider("Slider 2", &val_2, 0.0f, 100.0f, 50, 200);
    slider("Slider 3", &val_1, 0.0f, 100.0f, 50, 200);
    slider("Slider 4", &val_2, 0.0f, 100.0f, 50, 200);
    slider("Slider 5", &val_1, 0.0f, 100.0f, 50, 200);

    // End the section
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

void Video::shutdownImGui()
{
    // Cleanup ImGui resources
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
} // end shutdownImGui

//=====================================================================================

void Video::slider(const char* label, float* value, float min, float max, uint width, uint height)
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
    ImGui::SetCursorPos(ImVec2(start_x + slider_spacing + width, start_y)); // Move down by the height of the slider plus spacing
} // end slider
