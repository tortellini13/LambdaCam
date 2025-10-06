#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include "LambdaUtil.hpp"

struct Config
{
    /* Configuration parameters */

    // Integers
    int m_channels = 4;        // Number of microphone rows
    int n_channels = 4;        // Number of microphone columns

    int fft_frame_size = 1024; // FFT frame size
    int sample_rate = 48000;   // Audio sample rate

    int fov_theta = 30;        // Field of view in theta direction (horizontal)
    int fov_phi = 30;          // Field of view in phi direction (vertical)
    int angle_resolution = 2;  // Angle resolution for beamforming

    // Floats
    float mic_spacing = 0.030; // Spacing between microphones in meters

    float min = -70.0f;        // Minimum dB for heatmap
    float max = -20.0f;        // Maximum dB for heatmap
    float alpha = 0.5f;        // Alpha blending for heatmap overlay

    // Booleans
    bool audio_is_streaming = false; // Is audio currently streaming
    bool use_alsa = false;           // Use ALSA for audio input

    bool dark_mode = true;           // Dark or light mode for UI
    bool show_heatmap = true;        // Show heatmap overlay
    bool show_heatmap_legend = true; // Show heatmap legend
    bool show_max_cursor = true;     // Show cursor at max heatmap value
    
    // Strings
    std::string channel_order = "44a820b931ec64fd75"; // Packed channel order
    std::string recordings_dir = "recordings/";       // Directory to save recordings
    std::string wav_file_name = "test1k.wav";         // Name of the WAV file for audio recording

    // Macro to enable JSON serialization/deserialization
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Config,
        m_channels, n_channels,
        fft_frame_size, sample_rate,
        fov_theta, fov_phi, angle_resolution,
        mic_spacing,
        min, max, alpha,
        audio_is_streaming, use_alsa,
        dark_mode, show_heatmap, show_heatmap_legend, show_max_cursor,
        channel_order, recordings_dir, wav_file_name
    );
    
    // Default constructor
    Config() = default;

    // Write to JSON file
    void write()
    {
        // Create file to write to
        std::ofstream outFile("config.json");

        // Try to open file
        if (!outFile.is_open()) 
        {
            LUtil::error("Config", "Failed to open the config file for writing");
        }

        // Serialize the whole struct to JSON and write to file
        outFile << nlohmann::json(*this).dump(4);
        outFile.close();
    } // end write

    // Read from JSON file
    void read()
    {
        Config defaults; // default instance

        std::ifstream inFile("config.json");
        if (!inFile.is_open())
        {
            LUtil::error("Config", "Config file not found. Creating default file.");
            write();
            *this = defaults;
            return;
        }

        try
        {
            nlohmann::json j;
            inFile >> j;
            inFile.close();

            // Merge with defaults for missing keys
            nlohmann::json j_defaults = defaults;
            j_defaults.merge_patch(j); // merge_patch keeps defaults if keys are missing
            *this = j_defaults.get<Config>();
        }
        catch (const nlohmann::json::exception& e)
        {
            LUtil::error("Config", std::string("JSON parsing error: ") + e.what());
            *this = defaults;
        }
    } // end read

    // Resets active config to defaults
    void resetToDefaults()
    {

    } // end resetToDefaults

}; // end Config