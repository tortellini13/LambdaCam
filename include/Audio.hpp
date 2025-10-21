#pragma once

#include <thread>           // For multithreading
#include <atomic>           // For atomic variables
#include <mutex>
#include <condition_variable>
#include <alsa/asoundlib.h> // ALSA
#include <chrono>           // For time aligning AudioFile stream
#include "Structs.hpp"      // Custom structs and enums
#include "AudioFile.h"      // For reading from a wav file
#include "Config.hpp"       // Config struct

class Audio
{
public:
    // Constructor and destructor
    Audio(Config& global_config);
    ~Audio();

    // Initializes audio settings
    bool initAudio();

    // Starts and stops audio stream on a separate thread
    void startAudioStream();
    void stopAudioStream();

    // Read the next frame of audio (thread safe)
    bool getNextFrame(array3D<float>& out_buffer);

private:
    // Stream audio with ALSA
    void streamAudioALSA();

    // Stream audio with AudioFile
    void streamAudioFile();

    // Initialize ALSA and AudioFile
    bool initALSA();
    bool initAudioFile();

    // Saves audio to file
    void exportFromWavFile(const int channel); // Exports one channel from existing wav file

    // Global configuration object
    Config &config; // Reference to the global configuration object

    // Data buffers
    array3D<float> read_buffer;
    array3D<float> write_buffer;

    // General Member Variables
    array2D<int> channel_order;     // Physical channels may not be in correct order

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

    // Syncing reads and writes
    std::mutex audio_mutex;
    std::condition_variable audio_cv;
    bool frame_ready = false;

    int temp = 0;
};