// Libraries
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <ctime>
#include <chrono>
#include <iomanip>

#include "PARAMS.h"
#include "ALSA.h"
#include "Beamform.h"
#include "Video.h"
#include "Timer.h"
#include "wav.h"

using namespace std;

CONFIG configs(NUM_INT_CONFIGS, NUM_FLOAT_CONFIGS, NUM_BOOL_CONFIGS, NUM_STRING_CONFIGS);

int main()
{
    Mat frame;

    //=====================================================================================

    // Initialize ALSA and Beamform
    #ifdef ENABLE_AUDIO
    #ifdef ENABLE_ALSA
    ALSA ALSA(AUDIO_DEVICE_NAME);
    #endif // ENABLE_ALSA
    beamform beamform(1);
    #endif // ENABLE_AUDIO

    // Initialize video
    #ifdef ENABLE_VIDEO
    video video(RESOLUTION_WIDTH, RESOLUTION_HEIGHT, FRAME_RATE);
    #endif // ENABLE_VIDEO

    //=====================================================================================

    // Timer for testing
    timer main_loop_time("Main Loop");

    // Arrays to store data
    array3D<float> audio_data_buffer_1(M_AMOUNT, N_AMOUNT, FFT_SIZE);
    array3D<float> audio_data_buffer_2(M_AMOUNT, N_AMOUNT, FFT_SIZE);
    cv::Mat processed_data(NUM_THETA, NUM_PHI, CV_32FC1, cv::Scalar(0));

    // Clear buffers
    for (int m = 0; m < audio_data_buffer_1.dim_1; m++)
    {
        for (int n = 0; n < audio_data_buffer_1.dim_2; n++)
        {
            for (int b = 0; b < audio_data_buffer_1.dim_3; b++)
            {
                audio_data_buffer_1.at(m, n, b) = 0.0f;
                audio_data_buffer_2.at(m, n, b) = 0.0f;
            } // end b
        } // end n
    } // end m

    // Send configuration to ALSA and start recording audio
    #ifdef ENABLE_AUDIO
    #ifdef ENABLE_ALSA
    ALSA.setup();
    ALSA.start();
    #endif // ENABLE_ALSA
    // cout << "Audio setup complete.\n"; 

    beamform.setup();
    // cout << "Beamform setup complete.\n";

    #ifdef ENABLE_WAV
    WAV WAV;
    WAV.setup("test1k.wav");
    #endif // ENABLE_WAV
    #endif // ENABLE_AUDIO

    // Start video capture
    #ifdef ENABLE_VIDEO 
    video.startCapture();
    cout << "Video setup complete.\n";
    #endif // ENABLE_VIDEO

    int pcm_error;
 
    //=====================================================================================

    cout << "Starting main loop.\n";
    while(1)
    {
        
        main_loop_time.start();

        // Copy data from ring buffer and process beamforming
        #ifdef ENABLE_AUDIO
        #ifdef ENABLE_ALSA
        ALSA.copyRingBuffer(audio_data_buffer_1, audio_data_buffer_2);
        #endif // ENABLE_ALSA

        // Test data read from a wav file
        #ifdef ENABLE_WAV
        WAV.readWAV(audio_data_buffer_1, audio_data_buffer_2);
        #endif // ENABLE_WAV

        beamform.processData(processed_data, configs.i(bin), audio_data_buffer_1);
        #endif // ENABLE_AUDIO
        
        // Generate heatmap and ui then display the frame
        pcm_error = 0;

        #ifdef ENABLE_VIDEO
        // if (waitKey(1) >= 0) break;
        #ifdef ENABLE_AUDIO
        #ifdef ENABLE_ALSA
        pcm_error = ALSA.pcm_error;
        #endif // ENABLE_ALSA
        #endif // ENABLE_AUDIO
        if (video.processFrame(processed_data, pcm_error) == false) break;
        #endif // ENABLE_VIDEO

        main_loop_time.stop();
    } // end loop

    //=====================================================================================

    // Clean up and exit
    #ifdef ENABLE_AUDIO
    #ifdef ENABLE_ALSA
    ALSA.stop();
    #endif // ENABLE_ALSA
    #endif // ENABLE_AUDIO

    #ifdef ENABLE_VIDEO
    video.stopCapture();
    #endif // ENABLE_VIDEO
 
    return 0;
} // end main