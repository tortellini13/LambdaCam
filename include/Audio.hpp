#pragma once

#include <thread>      // For multithreading
#include <atomic>      // For atomic variables
#include <alsa/asoundlib.h> // ALSA
#include <chrono>      // For time aligning AudioFile stream
#include "Structs.hpp" // Custom structs and enums
#include "AudioFile.h" // For reading from a wav file

class Audio
{
public:
    // Constructor and destructor
    Audio(CONFIG& global_config);
    ~Audio();

    // Initializes audio settings
    bool initAudio();

    // Starts and stops audio stream on a separate thread
    void startAudioStream();
    void stopAudioStream();

    // Access ring buffer
    void accessRingBuffer(array3D<float>& data_output_1, array3D<float>& data_output_2);

private:
    // Stream audio with ALSA
    void streamAudioALSA();

    // Stream audio with AudioFile
    void streamAudioFile();

    // Initialize ALSA and AudioFile
    bool initALSA();
    bool initAudioFile();

    // Global configuration object
    CONFIG &config; // Reference to the global configuration object

    // General Member Variables
    int m_channels;                 // Number of microphones in the m direction
    int n_channels;                 // Number of microphones in the n direction
    array2D<int> channel_order;     // Physical channels may not be in correct order

    array3D<float> data_buffer_1;   // Buffer for audio data
    array3D<float> data_buffer_2;   // Buffer for audio data

    // Member variables for ALSA
    snd_pcm_t *pcm_handle;          // pcm handle
    snd_pcm_hw_params_t *hw_params; // Contains information about pcm configs
    const char *pcm_name;           // Name of pcm device (ie. hw:0,0)
    unsigned int exact_rate;        // Sample rate returned by snd_pcm_hw_params_rate_near
    int dir;                        // Checks if rate and exact_rate are the same
    snd_pcm_uframes_t frames;       // Number of frames recorded per period
    snd_pcm_uframes_t buffer_size;  // Size of buffer (BUFFER_SIZE * NUM_BYTES * NUM_CHANNELS)
    int32_t* input_buffer;          // Buffer for interlaced data to be written to
    int pcm_return;                 // Return value for pcm reading (for error handling)

    // Member variables for AudioFile
    AudioFile<double> audio_stream; // Audio stream

    // Multithreading
    thread streaming_thread;        // Thread for recording audio
    atomic<bool> is_streaming;      // Flag for recording status
    atomic<int> pcm_error = 0;      // Flag for buffer error
};