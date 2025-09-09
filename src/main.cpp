// Libraries
#include <iostream>

// Headers
#include "Audio.hpp"      // Audio streaming from ALSA or AudioFile
#include "Video.hpp"      // Camera and GUI rendering
#include "Beamform.hpp"   // Beamforming and audio processing
#include "LambdaUtil.hpp" // Utility functions for LambdaCam
#include "ConfigIO.hpp"   // Config file reading/writing
#include "ConfigKey.hpp"  // Names and keys for each config
#include "Structs.hpp"    // Custom structs

int main()
{
    // Create a gap in the console so it is easier to see output
    std::cout << "\n";

    array2D<int> channel_order_test(4, 4);

    channel_order_test.at(0, 0) = 10;
    channel_order_test.at(0, 1) = 8;
    channel_order_test.at(0, 2) = 2;
    channel_order_test.at(0, 3) = 0;

    channel_order_test.at(1, 0) = 11;
    channel_order_test.at(1, 1) = 9;
    channel_order_test.at(1, 2) = 3;
    channel_order_test.at(1, 3) = 1;

    channel_order_test.at(2, 0) = 14;
    channel_order_test.at(2, 1) = 12;
    channel_order_test.at(2, 2) = 6;
    channel_order_test.at(2, 3) = 4;

    channel_order_test.at(3, 0) = 15;
    channel_order_test.at(3, 1) = 13;
    channel_order_test.at(3, 2) = 7;
    channel_order_test.at(3, 3) = 5;

    std::string order = LUtil::packChannelOrder(channel_order_test);
    std::cout << "Order: " << order << "\n";

    //=====================================================================================

    /* Initialize configs */

    // Global config object
    CONFIG global_config(
        static_cast<size_t>(LKey::NUM_INT_CONFIGS), 
        static_cast<size_t>(LKey::NUM_FLOAT_CONFIGS), 
        static_cast<size_t>(LKey::NUM_BOOL_CONFIGS), 
        static_cast<size_t>(LKey::NUM_STRING_CONFIGS));

    // Read from default configs
    if (!ConfigIO::readConfig(global_config, true))
    {
        LUtil::error("Main", "Failed to read default config");
        return 1;
    }

    // Read from user configs and overwrite defaults if availible
    if (!ConfigIO::readConfig(global_config))
    {
        LUtil::error("Main", "Failed to read user configs");
        return 1;
    }

    // Write config to ensure it has all variables
    if (!ConfigIO::writeConfig(global_config))
    {
        LUtil::error("Main", "Failed to write config");
        return 1;
    } 

    //=====================================================================================

    /* Allocate memory for buffers */

    // Audio data buffers
    array3D<float> audio_data_buffer_1(
        global_config.i(LKey::M_CHANNELS), 
        global_config.i(LKey::N_CHANNELS), 
        global_config.i(LKey::FFT_FRAME_SIZE));
    array3D<float> audio_data_buffer_2(
        global_config.i(LKey::M_CHANNELS), 
        global_config.i(LKey::N_CHANNELS), 
        global_config.i(LKey::FFT_FRAME_SIZE));
    
    // Beamforming output buffer
    array2D<float> beamform_data_buffer(
        global_config.i(LKey::FOV_THETA),
        global_config.i(LKey::FOV_PHI));

    //=====================================================================================

    /* Initialize classes */

    // Test data***
    array2D<float> test_input_data(100, 100);
    test_input_data.fill(-45.0f);
    float time = 0.0f;

    // Initialize video class and dispatch thread to record video
    Video video; 
    if (!video.startVideo())
    {
        LUtil::error("Main", "Failed to start video capture");
        return 1;
    }

    // Initialize the audio class
    Audio audio(global_config);
    if (!audio.initAudio())
    {
        LUtil::error("Main", "Failed to start audio stream");
        return 1;
    }

    // Initialize the beamform class
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
    while (video.video_running)
    {
        LUtil::radialGradient(test_input_data, -100, 0, time); // Generate a radial gradient***testing

        // Copy audio stream into main thread
        audio.accessRingBuffer(audio_data_buffer_1, audio_data_buffer_2);

        // Perform beamforming algorithm to audio data
        beamform.processAudioFrame(audio_data_buffer_1, beamform_data_buffer, 40);

        // Draw the UI and create a heatmap
        // if(!video.processFrame(beamform_data_buffer, -100, 0, 0.5f))
        if (!video.processFrame(test_input_data, -100, 0, 0.5f))
        {
            LUtil::error("Main", "Failed to process frame");
            break;
        }
    }

    return 0;
} // end main