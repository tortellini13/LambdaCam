#pragma once

#include "Structs.h" // For CONFIG

// Mic channels
#define M_AMOUNT 4 // Amount of mics in the M direction
#define N_AMOUNT 4 // Amount of mics in the N direction
#define MIC_SPACING 0.030f // Spacing between microphones (m)
const int NUM_CHANNELS = (M_AMOUNT * N_AMOUNT); // Total number of channels
const int CHANNEL_ORDER[M_AMOUNT][N_AMOUNT] =
{
    {10, 8,  2, 0},
    {11, 9,  3, 1},
    {14, 12, 6, 4},
    {15, 13, 7, 5}
};

// Angles
#define MIN_THETA -30
#define MAX_THETA  30
#define STEP_THETA 2 // 2 is close to 1:1
const int NUM_THETA = ((MAX_THETA - MIN_THETA) / STEP_THETA) + 1;

#define MIN_PHI -18
#define MAX_PHI  18
#define STEP_PHI 1  // 1 is close to 1:1
const int NUM_PHI = ((MAX_PHI - MIN_PHI) / STEP_PHI) + 1;

// Audio
const char* AUDIO_DEVICE_NAME = "hw:0,0"; // arecord -l (type in console to find)
#define SAMPLE_RATE 48000                 // Audio sample rate (Hz)
#define FFT_SIZE 1024                     // Amount of samples in one frame of the FFT
#define SPEED_OF_SOUND 343.0f             // Speed of sound (m/s)

// Camera
#define FRAME_RATE 30         // Frame rate of the camera (fps)
#define RESOLUTION_WIDTH 640  // Width of the camera (px)
#define RESOLUTION_HEIGHT 480 // Height of the camera (px)

// Heatmap
#define MAP_THRESHOLD_TRACKBAR_VAL 0 // Initial threshold for heat map 
#define MAP_THRESHOLD_OFFSET 100     // Offset for trackbar position relative to threshold value (trackbar pos - offset = threshold)
#define MAP_THRESHOLD_MAX 0          // Maximum threshold for heat map
#define DEFAULT_ALPHA 60             // Default alpha value (ALPHA * 100)

// Text
#define FONT_TYPE FONT_HERSHEY_PLAIN // Font for overlayed text
#define FONT_THICKNESS 1             // Font thickness for overlayed text
#define FONT_SCALE 1                 // Font scale for overlayed text
#define MAX_LABEL_POS_X 10           // Horizontal location of maximum value overlayed text
#define MAX_LABEL_POS_Y 20           // Vertical location of maximum value overlayed text
#define LABEL_PRECISION 1            // Number of decimal places to be shown on screen

// Scale
#define SCALE_WIDTH 40   // Width of the color scale
#define SCALE_HEIGHT 400 // Height of the color scale
#define SCALE_POS_X 580  // X Position of color scale
#define SCALE_POS_Y 40   // Y Position of color scale
#define SCALE_BORDER 5   // Thickness of border around scale
#define SCALE_POINTS 1   // Quantity of points on the scale to be marked

// Crosshair
#define CROSS_THICKNESS 2 // Thickness for cross at maximum magnitude
#define CROSS_SIZE 20     // Size of cross at maximum magnitude

// FPS counter
#define FPS_COUNTER_AVERAGE 10 // Number of frames to be averaged for calculating FPS

// Configs
extern CONFIG configs;

enum int_configs: uint8_t
{
    
    quality,
    midpoint,
    octave_band_value,
    third_band_value,
    bin,
    NUM_INT_CONFIGS
};

enum float_configs: uint8_t
{
    imgui_alpha,
    imgui_clamp_min,
    imgui_clamp_max,
    imgui_threshold,
    NUM_FLOAT_CONFIGS
};

enum bool_configs: uint8_t
{
    mark_max_mag_state,
    color_scale_state,
    heat_map_state,
    data_clamp_state,
    threshold_state,

    auto_save_state,
    record_state,
    capture_image_state,

    random_state,
    static_state,

    options_menu,
    hidden_menu,

    full_range,
    octave_bands,
    normalize_mag,
    NUM_BOOL_CONFIGS
};

enum string_configs: uint8_t
{
    heatmap,
    save_path,
    current_band,
    NUM_STRING_CONFIGS
};

// For future use
enum FULL_OCTAVE_BANDS: uint8_t
{
    FULL_63,
    FULL_125,
    FULL_250,
    FULL_500,
    FULL_1000,
    FULL_2000,
    FULL_4000,
    FULL_8000,
    FULL_16000,
    NUM_FULL_OCTAVE_BANDS
};

enum THIRD_OCTAVE_BANDS: uint8_t
{
    // THIRD_20,
    // THIRD_25,
    // THIRD_31_5,
    // THIRD_40,
    // THIRD_50,
    THIRD_63,
    THIRD_80,
    THIRD_100,
    THIRD_125,
    THIRD_160,
    THIRD_200,
    THIRD_250,
    THIRD_315,
    THIRD_400,
    THIRD_500,
    THIRD_630,
    THIRD_800,
    THIRD_1000,
    THIRD_1250,
    THIRD_1600,
    THIRD_2000,
    THIRD_2500,
    THIRD_3150,
    THIRD_4000,
    THIRD_5000,
    THIRD_6300,
    THIRD_8000,
    THIRD_10000,
    THIRD_12500,
    THIRD_16000,
    // THIRD_20000,
    NUM_THIRD_OCTAVE_BANDS
};

// For debugging. Uncomment to enable
// #define PROFILE_MAIN
// #define PROFILE_BEAMFORM
// #define PROFILE_VIDEO
#define ENABLE_AUDIO
#define ENABLE_VIDEO
#define ENABLE_IMGUI
#define AVG_SAMPLES 10
#define ENABLE_ALSA
// #define ENABLE_WAV // Enable reading from wav file, must comment "#define ENABLE_AUDIO"