#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <atomic>
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/ImGuiFileDialog.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>

// add this to try on pi to makefile: sdl2-config --cflags

using namespace std;
using namespace cv;

class video 
{
public:
    video(int frame_width, int frame_height, int frame_rate);

    ~video();

    // Performd setup and dispatches camera thread
    void startCapture();

    // Stops the camera thread
    void stopCapture();

    // Performs video processing and draws graphics
    bool processFrame(Mat& data_input, int pcm_error);

private:
    // Video Functions
    bool getFrame(Mat& frame); // Copies frame from dispatched thread to main thread
    void captureVideo(VideoCapture& cap, atomic<bool>& is_running); // Captures video stream in a separate thread

    // OpenCV Functions
    Mat createHeatmap(Mat& data_input, const float lower_limit, const float upper_limit, Mat& frame); // Creates heatmap from input data, thesholds, clamps, and merges with video frame. Also contains generators for test data
    void drawColorBar(int scale_width, int scale_height); // Draws color bar onto frame
    Mat drawUI(Mat& data_input);      // Draws UI onto frame
    void saveImage(const Mat& frame); // Save image of screen

    // ImGui Functiions
    bool startImGui();               // Setup the imgui stuff
    bool renderImGui(Mat& frame_in); // Render each imgui window frame
    void shutdownImGui();            // Kill john lennon (imgui)
    void UISetup();                  // Setup the UI
    void FPSCalculator();            // Calculate the FPS
    bool readConfig();               // Read the config file
    bool writeConfig();              // Write the config file

    // Camera params
    int frame_width; // I dont remember why this are explicit but i had a reason
    int frame_height;
    int frame_rate;

    VideoCapture cap; // For capturing video

    // Multithreading
    atomic<bool> is_running; // Tracks if the camera thread is running
    thread video_thread;     // Thread for capturing video
    queue<Mat> frame_queue; // Queue for storing frames
    mutex frame_mutex;  // Protects access to the frame queue

    // Size for sliders
    int column_width = 10;
    int slider_width = 50;
    
    // Tracks the max and min values of the data in a frame
    double magnitude_min;
    double magnitude_max;
    Point max_coord;        // Coords from data
    Point max_point_scaled; // Coords scaled to frame

    // Color Bar
    Mat static_color_bar;
    Mat color_bar;
    Mat scaleColor;

    // FPS Tracker
    timer FPSTimer;    // FPS of video processing
    timer camFPSTimer; // FPS of camera
    double FPS;
    double camFPS;

    // Need to comment these better - ImGUI
    GLuint textureID; // Video frame ID that is to be converted to ImGui texture
    SDL_Window* window = nullptr;
    
    SDL_GLContext gl_context;

    SDL_Event event;

    Mat frame;

    // Config File
    unordered_map<string, string> config; // Buffer for the config file

    ifstream configfile;   // File to read from
    ofstream wrconfigfile; // File to write to

    string heatmap_style;  // COLORMAP_JET (Why is this here?)

    int loopcounter;       // Number of loops for deciding when to write the configuration to file again
    int save_counter = 0;  // Number of loops for deciding when to save the image

    // Flags
    int pcm_error;                    // Error code from ALSA (Do we need this?)
    bool fatal_error_flag = false;    // Error flag (From what?)
    bool missing_config_flag = false; // Config file missing
    bool was_error = false;           // Flag for persisting error window until "Okay" is pressed
    bool save_success = false;        // Saving was successful
    bool was_recording = false;       // Flag for stopping video recording

    // Frame buffers for ring buffer
    Mat frame_buffer_1;
    Mat frame_buffer_2;

    // Video recorder object
    VideoWriter videorec;

}; // end class def

//=====================================================================================

video::video(int frame_width, int frame_height, int frame_rate):
    frame_width(frame_width),
    frame_height(frame_height),
    frame_rate(frame_rate),
    cap("/dev/video0", CAP_V4L2),
    videorec("/", VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, Size(RESOLUTION_WIDTH, RESOLUTION_HEIGHT), true),
    FPSTimer("FPS Timer"),
    camFPSTimer("CAM FPS Timer") 
    {}

video::~video() 
{
    stopCapture();
}// end ~video

//=====================================================================================

bool video::readConfig() 
{
    // Open configuration file
    configfile.open("config.txt");

    // Write defaults into map
    string cfgline;

    // Load all defaults (Store defaults in PARAMS as a CONFIG struct to avoid this?)
    config["heatmap"]               = "COLORMAP_JET";
    config["mark_max_mag_state"]    = "true";
    config["color_scale_state"]     = "true";
    config["heat_map_state"]        = "true";
    config["data_clamp_state"]      = "true";
    config["threshold_state"]       = "true";
    config["auto_save_state"]       = "true";
    config["record_state"]          = "false";
    config["capture_image_state"]   = "false";
    config["random_state"]          = "false";
    config["static_state"]          = "false";
    config["options_menu"]          = "false";
    config["imgui_threshold"]       = to_string(-100);
    config["imgui_clamp_min"]       = to_string(-100);
    config["imgui_clamp_max"]       = to_string(0);
    config["imgui_alpha"]           = to_string(0.7);
    config["quality"]               = to_string(2);
    config["save_path"]             = "";
    config["full_range"]            = "true";
    config["octave_bands"]          = "false";
    config["octave_band_value"]     = to_string(1);
    config["third_band_value"]      = to_string(1);
    config["bin"]                   = to_string(100);
    config["third_band_value"]      = to_string(1);
    config["bin"]                   = to_string(21);
    config["midpoint"]              = to_string(90);
    config["normalize_mag"]         = "true";

    // Load current configuration
    if (configfile) 
    { 
        // Read the config files
        cout << "Now loading current config\n";
        while (getline(configfile, cfgline)) 
        { 
            cfgline.erase(0, cfgline.find_first_not_of(" \t\r\n")); // Trim leading spaces
            cfgline.erase(cfgline.find_last_not_of(" \t\r\n") + 1); // Trim trailing spaces
            if (cfgline.empty() || cfgline[0] == '#') continue; // Skip empty lines and comments
            size_t pos = cfgline.find('='); // Look for equal sign

            // Split up the "parameter"="value"
            if (pos != string::npos) 
            { 
                string key = cfgline.substr(0, pos);
                string value = cfgline.substr(pos + 1);
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                config[key] = value; // Write entry to the map
            }
        }
    } 

    // Restore default configuration
    else 
    {
        cout << "Restoring defualt configuration\n";
        missing_config_flag = true;
        wrconfigfile.open("config.txt");
        if(!wrconfigfile) {cout << "ERROR OPENING CONFIG.TXT FOR WRITING\n"; fatal_error_flag == true; return false;}
        for (const auto& pair : config) 
        {
            wrconfigfile << pair.first << "=" << pair.second << "\n";
        }
        wrconfigfile.close();
    }

    // Load these values into structure configs
    // For bools, the config[] stores true/false a string and not bool, so this is how I am converting it #sorrynotsorry

    configs.s(heatmap)              = config["heatmap"]; 
    configs.b(mark_max_mag_state)   = config["mark_max_mag_state"]  == "true";
    configs.b(color_scale_state)    = config["color_scale_state"]   == "true";
    configs.b(heat_map_state)       = config["heat_map_state"]      == "true";
    configs.b(data_clamp_state)     = config["data_clamp_state"]    == "true";
    configs.b(threshold_state)      = config["threshold_state"]     == "true";
    configs.b(auto_save_state)      = config["auto_save_state"]     == "true";
    configs.b(record_state)         = config["record_state"]        == "true";
    configs.b(capture_image_state)  = false;
    configs.b(random_state)         = config["random_state"]        == "true";
    configs.b(static_state)         = config["static_state"]        == "true";
    configs.b(options_menu)         = config["options_menu"]        == "true";
    configs.f(imgui_threshold)      = stof(config["imgui_threshold"]);
    configs.f(imgui_clamp_min)      = stof(config["imgui_clamp_min"]);
    configs.f(imgui_clamp_max)      = stof(config["imgui_clamp_max"]);
    configs.f(imgui_alpha)          = stof(config["imgui_alpha"]);
    configs.i(quality)              = stoi(config["quality"]);
    configs.s(save_path)            = config["save_path"];
    configs.b(full_range)           = config["full_range"]          == "true";
    configs.b(octave_bands)         = config["octave_bands"]        == "true";
    configs.i(octave_band_value)    = stoi(config["octave_band_value"]);
    configs.i(third_band_value)     = stoi(config["third_band_value"]);
    configs.i(bin)                  = stoi(config["bin"]);  
    configs.i(midpoint)             = stoi(config["midpoint"]);
    configs.b(normalize_mag)        = config["normalize_mag"]       == "true"; 

    return true;
} // end readConfig

//=====================================================================================

bool video::writeConfig() 
{    
    config["heatmap"]               = configs.s(heatmap);
    config["mark_max_mag_state"]    = configs.b(mark_max_mag_state) ? "true" : "false";
    config["color_scale_state"]     = configs.b(color_scale_state) ? "true" : "false";
    config["heat_map_state"]        = configs.b(heat_map_state) ? "true" : "false";
    config["data_clamp_state"]      = configs.b(data_clamp_state) ? "true" : "false";
    config["threshold_state"]       = configs.b(threshold_state) ? "true" : "false";
    config["auto_save_state"]       = configs.b(auto_save_state) ? "true" : "false";
    config["record_state"]          = configs.b(record_state) ? "true" : "false";
    config["capture_image_state"]   = configs.b(capture_image_state) ? "true" : "false";
    config["random_state"]          = configs.b(random_state) ? "true" : "false";
    config["static_state"]          = configs.b(static_state) ? "true" : "false";
    config["options_menu"]          = configs.b(options_menu) ? "true" : "false";
    config["imgui_threshold"]       = to_string(configs.f(imgui_threshold));
    config["imgui_clamp_min"]       = to_string(configs.f(imgui_clamp_min));
    config["imgui_clamp_max"]       = to_string(configs.f(imgui_clamp_max));
    config["imgui_alpha"]           = to_string(configs.f(imgui_alpha));
    config["quality"]               = to_string(configs.i(quality));
    config["save_path"]             = configs.s(save_path);
    config["full_range"]            = configs.b(full_range) ? "true" : "false";
    config["octave_bands"]          = configs.b(octave_bands) ? "true" : "false";
    config["octave_band_value"]     = to_string(configs.i(octave_band_value));
    config["third_band_value"]      = to_string(configs.i(third_band_value));
    config["bin"]                   = to_string(configs.i(bin)); 
    config["midpoint"]               = to_string(configs.i(midpoint));
    config["normalize_mag"]        = configs.b(normalize_mag) ? "true" : "false";

    wrconfigfile.open("config.txt");
    if(!wrconfigfile) {cout << "ERROR OPENING CONFIG.TXT FOR WRITING\n"; fatal_error_flag = true; return false;}
    for (const auto& pair : config) 
    {
        wrconfigfile << pair.first << "=" << pair.second << "\n";
    }
    wrconfigfile.close();

    return true;
} // end writeConfig

//=====================================================================================

void video::startCapture() 
{
    if (!readConfig()) 
    {
        cout << "Could not read config.....\n";
    }

    // Configure capture properties
    cap.open("/dev/video0", CAP_V4L2);
    cap.set(CAP_PROP_FRAME_WIDTH, frame_width);
    cap.set(CAP_PROP_FRAME_HEIGHT, frame_height);
    cap.set(CAP_PROP_FPS, frame_rate);
    cap.set(CAP_PROP_BUFFERSIZE, 1);

    // Record video on separate thread
    is_running = true;
    video_thread = thread(&video::captureVideo, this, ref(cap), ref(is_running));

    UISetup();
} // end startCapture

void video::stopCapture() 
{
    is_running = false;
    if (video_thread.joinable()) 
    {
        video_thread.join();
    }

    destroyAllWindows();
    shutdownImGui();
    
} // end stopCapture

//=====================================================================================

void video::UISetup() 
{   
    // Make a window for the buttons to go on
    drawColorBar(SCALE_WIDTH, SCALE_HEIGHT);

    // Try to start ImGui
    if (startImGui() == false) 
    {
        cout << "IMGui init FAILED :(\n";
    }

    Mat initial_frame(Size(RESOLUTION_HEIGHT, RESOLUTION_WIDTH), CV_32FC1);

    while (!getFrame(initial_frame)) 
    {
        cerr << "Error: Could not retreive frame from capture thread!\n";
        this_thread::sleep_for(chrono::seconds(1));
    }

    FPSTimer.start();

    cout << "Using backend: " << cap.getBackendName() << "\n";
    cout << "Frame width: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "\n";
    cout << "Frame height: " << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";

} // end UISetup

//=====================================================================================

void video::captureVideo(VideoCapture& cap, atomic<bool>& is_running) 
{
    while (is_running) 
    {
        camFPSTimer.start();
        Mat temp_frame;

        if (cap.read(temp_frame)) 
        {
            swap(frame_buffer_1, frame_buffer_2); // Swap buffers
            temp_frame.copyTo(frame_buffer_2);    // Copy the new frame to the buffer
        }

        else 
        {
            cerr << "Error: Could not capture frame from video source!\n";
            is_running = false;
        }
        camFPSTimer.stop();
    }
} // end videoCapture

//=====================================================================================

bool video::getFrame(Mat& frame) 
{
    // Try to fetch a frame without blocking
    lock_guard<mutex> lock(frame_mutex);
    if (is_running) 
    {
        frame_buffer_1.copyTo(frame); // Copy the frame to the output
        return true;
    }
    return false; // No frame available
} // end getFrame

//=====================================================================================

Mat video::createHeatmap(Mat& data_input, const float lower_limit, const float upper_limit, Mat& frame)
{
    // Ensure data_input is of type CV_32F for proper normalization
    Mat workingFrame;
    frame.copyTo(workingFrame);
    
    // Random data generation
    if(configs.b(random_state) == true) {randu(data_input, Scalar(-100), Scalar(0));}

    // Display static test frame
    if(configs.b(static_state) == true) 
    {
    double st_height = NUM_PHI;
    double st_width = NUM_THETA;
    double st_max = 0;
    double st_min = -100;

    array2D<float> static_test_frame(st_width, st_height);

    for (int i = 0; i < static_test_frame.dim_1; i++)
    {
        for (int j = 0; j < static_test_frame.dim_2; j++)
        {
            // Calculate distance from center and assign value based on distance
            float value;
            float distance = sqrt(((st_width / 2 - i)*(st_width / 2 - i)) + ((st_height / 2 - j)*(st_height / 2 - j)));
            if (distance == st_width) {value = st_min;} 
            else {value = (static_cast<float>(st_max - st_min) * (1 - static_cast<float>(distance / st_width))) + st_min;}

            // Assign value to the static test frame
            static_test_frame.at(i, j) = value;
        } // end j
    } // end i

    // Create a Mat and reassign data
    Mat mat(static_test_frame.dim_1, static_test_frame.dim_2, CV_32FC1, static_test_frame.data);

    mat.copyTo(data_input);
    }

    // Clamp data to min and max values
    if (configs.b(data_clamp_state) == true) 
    {
        Mat data_clamped;
        data_input.convertTo(data_clamped, CV_32F);

        if (configs.f(imgui_clamp_min) >= configs.i(imgui_clamp_max)) 
        {
            if(configs.f(imgui_clamp_min) == 0) 
            {
                configs.f(imgui_clamp_min) = -1;
                configs.f(imgui_clamp_max) = 0;
            } 
            else if(imgui_clamp_max == -100) 
            {
                configs.f(imgui_clamp_max) = -99;
                configs.f(imgui_clamp_min) = -100;
            } 
            else 
            {
                configs.f(imgui_clamp_max) = configs.f(imgui_clamp_min) + 1;
            }

        }

        for(int col = 0; col < data_clamped.cols; col++ ) 
        {
            for (int row = 0; row < data_clamped.rows; row++) 
            {

                if(data_clamped.at<float>(row, col) < configs.f(imgui_clamp_min)) 
                {
                    data_clamped.at<float>(row, col) = configs.f(imgui_clamp_min);
                }

                if(data_clamped.at<float>(row, col) > configs.f(imgui_clamp_max)) 
                {
                data_clamped.at<float>(row, col) = configs.f(imgui_clamp_max);
                }   

            }
        }

    data_clamped.copyTo(data_input);
    }

    // Normalizes data to be 0-255
    Mat data_normalized;
    Mat data_midpointed;
    
    data_input.convertTo(data_normalized, CV_32F);
  
    // Find coord of max and min magnitude
    minMaxLoc(data_input, &magnitude_min, &magnitude_max, NULL, &max_coord);
    
    // Scale max point to video resolution
    max_point_scaled.x = (static_cast<double>(max_coord.x) / static_cast<double>(data_normalized.cols)) * RESOLUTION_WIDTH; 
    max_point_scaled.y = (static_cast<double>(max_coord.y) / static_cast<double>(data_normalized.rows)) * RESOLUTION_HEIGHT;
    
    if (configs.b(normalize_mag) == true) 
    {
        normalize(data_input, data_normalized, 0, 255, NORM_MINMAX);
    } 
    else 
    {
        for (int y = 0; y < data_normalized.rows; y++) 
        {
            for (int x = 0; x < data_normalized.cols; x++) 
            {
                if (data_input.at<float>(y, x) < configs.f(imgui_clamp_min)) 
                {
                    data_normalized.at<float>(y, x) = 0;
                } 
                else if (data_input.at<float>(y, x) > configs.f(imgui_clamp_max)) 
                {
                    data_normalized.at<float>(y, x) = 255;
                } 
                else 
                {  
                data_normalized.at<float>(y, x) = (data_input.at<float>(y, x) -  magnitude_min) * 255 / (magnitude_max - magnitude_min);
                }
            }
        }
    }

    int midpoint_scale = (configs.i(midpoint) * 255) / 100;

    data_normalized.copyTo(data_midpointed);

    // For midpoint scaling
    for (int y = 0; y < data_normalized.rows; y++) 
    {
        for (int x = 0; x < data_normalized.cols; x++) 
        {
            // Scale 0–midpoint to 0–127
            if (data_normalized.at<float>(y, x) <= midpoint_scale) 
            {
                data_midpointed.at<float>(y, x) = 127.0f * (data_normalized.at<float>(y, x) / midpoint_scale);
            } 

            // Scale midpoint–255 to 127–255
            else 
            {
                data_midpointed.at<float>(y, x) = 127.0f + 128.0f * ((data_normalized.at<float>(y, x) - midpoint_scale) / (255.0f - midpoint_scale));
            }
        }
    }

    // Convert to 8-bit format (required for applying color map)
    Mat heatmap_input;
    data_midpointed.convertTo(heatmap_input, CV_8U);

    // Apply a colormap to create the heatmap
    Mat heatmap;
    applyColorMap(heatmap_input, heatmap, COLORMAP_JET);

    Mat frame_merged;
    
    if (configs.b(heat_map_state) == false) 
    {   
        workingFrame.copyTo(frame_merged);
    }

    // Ensure the heatmap is the same size as the frame
    if (configs.b(heat_map_state) == true) 
    {
        Mat resized_heatmap;
        Mat resized_data_input;
        resize(heatmap, resized_heatmap, workingFrame.size());
        resize(data_input, resized_data_input, workingFrame.size());

        // Blend the heatmap and the frame with an alpha value
        if (configs.b(threshold_state) == false) 
        {
            addWeighted(workingFrame, 1.0, resized_heatmap, configs.f(imgui_alpha), 0.0, frame_merged);
        }
        if (configs.b(threshold_state) == true) 
        {
            for (int y = 0; y < resized_heatmap.rows; y++)
                {
                    for (int x = 0; x < resized_heatmap.cols; x++) 
                    {
                        if (resized_data_input.at<float>(y, x) > configs.f(imgui_threshold)) 
                        {
                            Vec3b& pixel = workingFrame.at<Vec3b>(y, x);          // Current pixel in camera
                            Vec3b heatMapPixel = resized_heatmap.at<Vec3b>(y, x); // Current pixel in heatmap

                            // Loops through RGB channels
                            for (int c = 0; c < 3; c++) 
                            {
                                // Merges heatmap with video and applies alpha value
                                pixel[c] = static_cast<uchar>(configs.f(imgui_alpha) * heatMapPixel[c] + (1 - configs.f(imgui_alpha)) * pixel[c]);
                            } 
                        }
                    }
                } // end alphaMerge
            workingFrame.copyTo(frame_merged);
        }
    }

    // Return the generated heatmap
    return frame_merged; 
} // end createHeatmap

//=====================================================================================

void video::drawColorBar(int scale_width, int scale_height) 
{
    color_bar = Mat(scale_height, scale_width, CV_8UC3);
    scaleColor = Mat(scale_height, scale_width, CV_8UC1, Scalar(0));
    cout << scaleColor.size() << "\n";

    for (int y = 0; y < scale_height; y++) 
    {
        int scaleIntensity = 255 * (static_cast<double>((scale_height- y)) / static_cast<double>(scale_height));
        
        for(int x = 0; x < scale_width; x++) 
        {
            scaleColor.at<uchar>(y, x) = scaleIntensity;
        }
    }
    cout << scale_width << " " << scale_height << "\n";
    applyColorMap(scaleColor, color_bar, COLORMAP_JET);
} // end drawColorBar

//=====================================================================================

void video::FPSCalculator()
{
    FPSTimer.stop();
    double FPSTime = FPSTimer.time_avg(AVG_SAMPLES, false);
    if (FPSTime != -1) {FPS = 1/FPSTime;}
    FPSTimer.start();

    double camFPSTime = camFPSTimer.time_avg(AVG_SAMPLES, false);
    if (camFPSTime != -1) {camFPS = 1/camFPSTime;}

}   

//=====================================================================================

Mat video::drawUI(Mat& data_input)
{
    // Color Bar Scale
    if (configs.b(color_scale_state) == true) 
    {   
        Rect color_bar_location(SCALE_POS_X, SCALE_POS_Y, SCALE_WIDTH, SCALE_HEIGHT);
        rectangle(data_input, Point(SCALE_POS_X, SCALE_POS_Y) + Point(-SCALE_BORDER, -SCALE_BORDER - 10), Point(SCALE_POS_X, SCALE_POS_Y) + Point(SCALE_WIDTH + SCALE_BORDER - 1, SCALE_HEIGHT + SCALE_BORDER + 5), Scalar(0, 0, 0), FILLED); //Draw rectangle behind scale to make a border
        
        color_bar.copyTo(data_input(color_bar_location)); //Copy the scale onto the image

        // Draw text indicating various points on the scale
        float scaleTextRatio = (1 / static_cast<float>(SCALE_POINTS ));
        for (int i = 0; i < (SCALE_POINTS + 1); i++) 
        {
            Point scaleTextStart(SCALE_POS_X - 6, (SCALE_POS_Y + ((1 - static_cast<double>(i) * scaleTextRatio) * SCALE_HEIGHT) - 3)); // Starting point for the text
            double scaleTextValue = ((static_cast<double>(magnitude_max - magnitude_min) * scaleTextRatio * static_cast<double>(i)) + magnitude_min); // Value of text for each point

            ostringstream scaleTextStream;
            String scaleTextString;
            String scaleTextSymbol = " ";

            if (configs.b(data_clamp_state) == 1) 
            {
                if (i == 0) 
                {
                    scaleTextSymbol = "<";
                }
                if (i == SCALE_POINTS) 
                {
                    scaleTextSymbol = ">";
                }
            }
            scaleTextStream << fixed << setprecision(LABEL_PRECISION) << scaleTextValue;
            scaleTextString = scaleTextSymbol + scaleTextStream.str() + " ";

            putText(data_input, scaleTextString, scaleTextStart, FONT_TYPE, FONT_SCALE - 0.2, Scalar(255, 255, 255), FONT_THICKNESS);
            line(data_input, scaleTextStart + Point(0,3), scaleTextStart + Point(SCALE_WIDTH + 6, 3), Scalar(255, 255, 255), 1, 8, 0);
        }
    } // end colar bar scale
    
    // Mark maximum location
    if (configs.b(mark_max_mag_state) == true) 
    {
        drawMarker(data_input, max_point_scaled, Scalar(0, 0, 0), MARKER_CROSS, CROSS_SIZE + 1, CROSS_THICKNESS + 1, 8); // Mark the maximum magnitude point
        drawMarker(data_input, max_point_scaled, Scalar(255, 255, 255), MARKER_CROSS, CROSS_SIZE, CROSS_THICKNESS, 8); // Mark the maximum magnitude point
    }
    
    return data_input;
} // end drawUI

//=====================================================================================

bool video::startImGui() 
{   
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) 
    {
        cerr << "Error: SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Set OpenGL ES attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    // Create an OpenGL ES window
    window = SDL_CreateWindow("Acoustic Camera", 1, 1, 1024, 550, SDL_WINDOW_OPENGL);
    if (!window) 
    {
        cerr << "Error: SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Create OpenGL ES context
    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) 
    {
        cerr << "Error: SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_GL_MakeCurrent(window, gl_context);

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io; // Avoid unused variable warning

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup platform/renderer bindings
    if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_context)) 
    {
        cerr << "Error: ImGui_ImplSDL2_InitForOpenGL failed!\n";
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) 
    {
        cerr << "Error: ImGui_ImplOpenGL3_Init failed!\n";
        return false;
    }

    cout << "Finished starting ImGui...\n";
    
    // Create texture for displaying each frame of video feed
    
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Allocate memory for texture (will be updated every frame)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RESOLUTION_WIDTH, RESOLUTION_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    cout << "Finished allocating Texture!!!!!!\n";
    
    return true;
} // end startImGui

//=====================================================================================

bool video::renderImGui(Mat &frame_in) 
{
    // Start a new frame
    SDL_Event event;
    while (SDL_PollEvent(&event)) 
    {
        ImGui_ImplSDL2_ProcessEvent(&event); // Pass events to ImGui

        if (event.type == SDL_QUIT)
        {
            return false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) 
        {
            return false;
        }
    }

    // Start new ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();

    Mat frameRGBA;

    // Create an empty RGBA image with the same size as the BGR image
    frameRGBA = Mat(frame_in.rows, frame_in.cols, CV_8UC4);

    // Iterate over each pixel in the BGR image and copy it to the RGBA image
    for (int i = 0; i < frame_in.rows; i++) 
    {
        for (int j = 0; j < frame_in.cols; j++) 
        {
            frameRGBA.at<Vec4b>(i, j) = Vec4b(frame_in.at<Vec3b>(i, j)[2], frame_in.at<Vec3b>(i, j)[1], frame_in.at<Vec3b>(i, j)[0], 255);
        }
    }


    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameRGBA.cols, frameRGBA.rows, GL_RGBA, GL_UNSIGNED_BYTE, frameRGBA.data);
    glBindTexture(GL_TEXTURE_2D, 0);
   
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(660, 500), ImGuiCond_Always);
    ImGui::Begin("Video", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    ImGui::Image((ImTextureID)(intptr_t)textureID, ImVec2(frame.cols, frame.rows));
    ImGui::End();

    // Error window
    if ((!pcm_error == 0) || (was_error == true) || (fatal_error_flag == true) || (missing_config_flag == true)) 
    {
        was_error = true;
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(660, 500), ImGuiCond_Always);
        ImGui::Begin("Error", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        if (missing_config_flag == true) 
        {
            ImGui::Text("Warning: Config file not found. Restoring defaults.");
        }
        if (fatal_error_flag == true) 
        {
            ImGui::Text("Fatal Error.");
        }
        switch(pcm_error) 
        {
            case 1: ImGui::Text("Error: Buffer overrun/underrun occurred."); break;
            case 2: ImGui::Text("Error: Error reading PCM data"); break;
            case 3: ImGui::Text("Error: PCM read returned fewer frames than expected."); break;
        }
        //ImGui::Text("Error: Could not capture frame from video source!");
        if(pcm_error == 0 && fatal_error_flag == false) {if (ImGui::Button("Ok")) {was_error = false;}}
        if(fatal_error_flag == true) {if (ImGui::Button("Quit :(")) {return false;}}
        ImGui::End();
    }
    
    // Slider Menu
    if (configs.b(options_menu) == false) 
    {
        ImGui::SetNextWindowPos(ImVec2(660, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(364, 550), ImGuiCond_Always);
        ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        
        // Define a constant width for sliders and labels to maintain uniformity
        ImGui::Text("FPS Counters:");
        ImGui::BeginGroup();
        ImGui::Text("Gui: %.1f", FPS);
        ImGui::SameLine();
        ImGui::Text("Camera: %.1f", camFPS);
        ImGui::EndGroup();

        ImGui::BeginGroup(); // First Column: Threshold
        ImGui::Text("Threshold");
        ImGui::VSliderFloat("##Threshold", ImVec2(slider_width, 425), &configs.f(imgui_threshold), configs.f(imgui_clamp_min), configs.f(imgui_clamp_max), "%.1f");
        ImGui::EndGroup();
        
        ImGui::SameLine(0, column_width); // Add some space between columns
        
        ImGui::BeginGroup(); // Second Column: Clamp Min
        ImGui::Text("Min");
        ImGui::VSliderFloat("##Clamp_Min", ImVec2(slider_width, 425), &configs.f(imgui_clamp_min), -100.0f, 0.0f, "%.1f");
        ImGui::EndGroup();
        
        ImGui::SameLine(0, column_width); // Add some space between columns
        
        ImGui::BeginGroup(); // Third Column: Clamp Max
        ImGui::Text("Max");
        ImGui::VSliderFloat("##Clamp_Max", ImVec2(slider_width, 425), &configs.f(imgui_clamp_max), -100.0f, 0.0f, "%.1f");
        ImGui::EndGroup();
        
        ImGui::SameLine(0, column_width + 7); // Add some space between columns
        
        int bin_freq = configs.i(bin)*SAMPLE_RATE/1024;

        ImGui::BeginGroup(); // Fifth Column: Quality
        ImGui::Text("%d", bin_freq);
        ImGui::VSliderInt("##Bin", ImVec2(slider_width, 425), &configs.i(bin), 1, 512, "%d");
        ImGui::EndGroup();

        ImGui::SameLine(0, column_width + 7); // Add some space between columns

        ImGui::BeginGroup(); // Fifth Column: Quality
        ImGui::Text("MidPoint");
        ImGui::VSliderInt("##Midpoint", ImVec2(slider_width, 425), &configs.i(midpoint), 1, 99, "%d");
        ImGui::EndGroup();

        ImGui::End(); // End the window
    }  

    ImGui::SetNextWindowPos(ImVec2(0,500), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(660, 50), ImGuiCond_Always);
    ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::BeginGroup();
    ImGui::Text("Maximum: %.1f", magnitude_max);
    ImGui::SameLine();

    ImGui::Checkbox("Options", &configs.b(options_menu));
    ImGui::SameLine();
    ImGui::Checkbox("Record", &configs.b(record_state));
    ImGui::SameLine();

    if(ImGui::Button("Capture Image")) 
    {
        configs.b(capture_image_state) = true;
    }
    if (save_success) 
    {
        ImGui::SameLine();
        ImGui::Text("Image Saved!");
        save_counter++;
    }
    if (save_counter == 30) 
    {
        save_success = false;
        save_counter = 0;
    }

    ImGui::EndGroup();

    ImGui::End();

    // Options menu
    if (configs.b(options_menu) == true) 
    {

    ImGui::SetNextWindowPos(ImVec2(660,0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(364, 550), ImGuiCond_Always);
    ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Checkbox("Mark Max Mag", &configs.b(mark_max_mag_state));
    ImGui::Checkbox("Color Scale", &configs.b(color_scale_state));
    ImGui::Checkbox("Heat Map", &configs.b(heat_map_state));
    ImGui::Checkbox("Data Clamp", &configs.b(data_clamp_state));
    ImGui::Checkbox("Threshold", &configs.b(threshold_state));
    ImGui::Checkbox("AutoSave Config", &configs.b(auto_save_state));
    ImGui::Checkbox("Normalize Data", &configs.b(normalize_mag));

    if (ImGui::Button("Select Save Directory")) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(1024, 500), ImGuiCond_Always);
        
        IGFD::FileDialogConfig configfig;
        configfig.path = ".";
        
        ImGuiFileDialog::Instance()->OpenDialog("ChooseDirDlgKey", "Choose Directory", nullptr, configfig);

    }

    if (ImGuiFileDialog::Instance()->Display("ChooseDirDlgKey")) 
    {
        if (ImGuiFileDialog::Instance()->IsOk()) 
        {
            configs.s(save_path) = ImGuiFileDialog::Instance()->GetCurrentPath();
            cout << "Selected directory: " << configs.s(save_path) << "\n";
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::NewLine();

    ImGui::BeginGroup(); // Fourth Column: Alpha
    ImGui::Text("Alpha");
    ImGui::VSliderFloat("##Alpha", ImVec2(slider_width, 200), &configs.f(imgui_alpha), 0.0f, 1.0f, "%.2f");
    ImGui::EndGroup();

    ImGui::End();
    } 
 
    // Render UI
    ImGui::Render();
    glViewport(0, 0, 1024, 550); 
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);
    // Write config if the lord sees it fit
    loopcounter++;
        if ((loopcounter == 300) && (configs.b(auto_save_state))) 
        {
            loopcounter = 0;

            if(!writeConfig()) 
            {
                cout << "ERROR WRITING CONFIG\n";
            }
        }

    return true;
} // end renderImGui

//=====================================================================================

void video::shutdownImGui() 
{
    if(configs.b(auto_save_state)){writeConfig();}
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
} // end shutdownImGui

//=====================================================================================

void video::saveImage(const Mat& image) 
{
    // Get the current time
    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);

    // Convert time to a string
    stringstream ss;
    ss << put_time(localtime(&in_time_t), "%Y%m%d%H%M%S");

    // Create a unique filename
    string filename = configs.s(save_path) + "/image_" + ss.str() + ".png";

    // Save the image
    if (imwrite(filename, image)) 
    {
        cout << "Image saved as " << filename << "\n";
        save_success = true;
    } 
    else 
    {
        cerr << "Error: Could not save image\n";
    }
} // end saveImage

//=====================================================================================

bool video::processFrame(Mat& data_input, int pcm_error_in)
{
    pcm_error = pcm_error_in;
    Mat newframe;
    
   if (getFrame(newframe)) 
   {
    newframe.copyTo(frame);
   }

   if(frame.empty()) 
   {
       cout << "Frame is empty\n";
       frame = Mat::zeros(RESOLUTION_HEIGHT, RESOLUTION_WIDTH, CV_8UC3);
       }

    // Creates heatmap from beamformed audio data, thresholds, clamps, and merges
    flip(data_input, data_input, 1); // Bop it
    Mat frame_merged = createHeatmap(data_input, 0.0f, 0.0f, frame);
        
    // Draw UI onto frame
    Mat frame_merged_UI = drawUI(frame_merged);
    
    FPSCalculator();

    if (configs.b(capture_image_state) == true) 
    {
        saveImage(frame_merged_UI);
        configs.b(capture_image_state) = false;
    }

    if (configs.b(record_state) == true) 
    {
        if (!was_recording) 
        {
            const char* path = configs.s(save_path).c_str();
            if (!path) 
            {
                cerr << "Error: save_path is null!\n";
            }

            // Convert time to a string
            auto now = chrono::system_clock::now();
            auto in_time_t = chrono::system_clock::to_time_t(now);
            stringstream ss;
            ss << put_time(localtime(&in_time_t), "%Y%m%d%H%M%S");

            // Create a unique filename
            string filename = configs.s(save_path) + "/video_" + ss.str() + ".avi";
            videorec.open(filename, VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, Size(RESOLUTION_WIDTH, RESOLUTION_HEIGHT), true);
            was_recording = true;

            if (!videorec.isOpened()) 
            {
                cerr << "Error: Could not open video file for recording!\n";
               was_recording = false;
               configs.b(record_state) = false;
            }
        }

        if (was_recording) 
        {
            // Write the frame to the video file
            videorec.write(frame_merged_UI);
        }
    }

    if (was_recording && configs.b(record_state) == false) 
    {
        // Stop recording
        videorec.release();
        was_recording = false;
    }

    if (renderImGui(frame_merged_UI) == false) 
    {
        return false;
    }
    
    return true;
} // end processData