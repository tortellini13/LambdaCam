// Libraries
#include <iostream>

// Headers
#include "Audio.hpp"      // Audio streaming from ALSA or AudioFile
#include "Video.hpp"      // Camera and GUI rendering
#include "Beamform.hpp"   // Beamforming and audio processing
#include "LambdaUtil.hpp" // Utility functions for LambdaCam
#include "Config.hpp"     // Config struct
#include "Structs.hpp"    // Custom structs

int main()
{
    // Create a gap in the console so it is easier to see output
    std::cout << "\n";

    //=====================================================================================

    /* Initialize configs */

    std::cout << "Initializing configs...\n";
    Config global_config;  // Create global config object with defaults
    global_config.read();  // Read from config file if available
    global_config.write(); // Write to config file to ensure all variables are present
    
    //=====================================================================================

    /* Initialize classes */

    // Test data***
    array2D<float> test_input_data(global_config.fov_theta, global_config.fov_phi);
    test_input_data.fill(-45.0f);
    float time = 0.0f;

    // Initialize video class and dispatch thread to record video
    std::cout << "Initializing video...\n";
    Video video(global_config); 
    if (!video.startVideo())
    {
        LUtil::error("Main", "Failed to start video capture");
        return 1;
    }

    // Initialize the audio class
    std::cout << "Initializing audio...\n";
    Audio audio(global_config);
    if (!audio.initAudio())
    {
        LUtil::error("Main", "Failed to start audio stream");
        return 1;
    }

    // Initialize the beamform class
    std::cout << "Initializing beamforming...\n";
    Beamform beamform(global_config);
    if (!beamform.initBeamform())
    {
        LUtil::error("Main", "Failed to initialize beamforming");
        return 1;
    }
    
    //=====================================================================================

    time = 0.05f;
    // LUtil::radialGradient(test_input_data, -100, 0, time);

    // Dispatch a thread to stream audio from ALSA or AudioFile
    audio.startAudioStream();

    std::cout << "\n==================== Starting main loop ====================\n";
    // Main loop
    array3D<float> audio_frame(global_config.m_channels, global_config.n_channels, global_config.fft_frame_size);
    while (video.video_running)
    {
        LUtil::radialGradient(test_input_data, 0, -100, time); // Generate a radial gradient***testing

        // Perform beamforming algorithm to audio data
        if (!audio.getNextFrame(audio_frame))
            LUtil::error("Main", "Failed to get audio frame");

        beamform.processAudioFrame(audio_frame, 22); // 1kHz bin

        // Draw the UI and create a heatmap
        if(!video.processFrame(beamform.output_buffer))
        // if (!video.processFrame(test_input_data))
        {
            LUtil::error("Main", "Failed to process frame");
            break;
        }
    }

    return 0;
} // end main