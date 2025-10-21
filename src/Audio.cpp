// Libraries
#include <iostream> 
#include <alsa/asoundlib.h>

// Headers
#include "Audio.hpp"
#include "Structs.hpp"
#include "LambdaUtil.hpp"
#include "AudioFile.h"

//=====================================================================================

/* Class constructors and destructors */

Audio::Audio(Config& global_config) :   
    config(global_config), // Reference to the global configuration object

    read_buffer(global_config.m_channels, global_config.n_channels, global_config.fft_frame_size), // Buffer for audio data
    write_buffer(global_config.m_channels, global_config.n_channels, global_config.fft_frame_size), // Buffer for audio data

    channel_order(config.m_channels, config.n_channels), // Physical channel order to remap

    pcm_name("hw:0,0"),            // Default ALSA device
    frames(config.fft_frame_size), // Number of frames per period
    buffer_size(config.m_channels * config.n_channels * frames)  // Size of buffer from ALSA

{} // end Audio constructor

Audio::~Audio()
{
    stopAudioStream(); // Stop audio stream

    delete[] input_buffer; // Free input buffer memory
    std::cout << "Audio object destroyed and resources cleaned up\n";
} // end Audio destructor

//=====================================================================================

/* Methods to initialize ALSA and AudioFile */

// Initialize ALSA with default settings
bool Audio::initALSA()
{
    // Boilerplate configs for ALSA. Will change with new audio interface***
    snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;        // Set the pcm stream to capture
    snd_pcm_access_t access = SND_PCM_ACCESS_RW_INTERLEAVED; // Stores data where ch1[0], ch2[0], ...ch16[0], ch1[1],...
    snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;         // Format for input data (32-bit little endian)
    int mode = 0;      // Mode for pcm (0 is default)
    int periods = 2;   // Number of periods. For scheduling interrupts
    
    // Allocate hw_params on the stack
    snd_pcm_hw_params_alloca(&hw_params);

    // Open pcm
    if (snd_pcm_open(&pcm_handle, pcm_name, stream, mode) < 0)
    {
        LUtil::error("Audio", "Failed to open PCM device");
        return false;
    }
    
    // Initialize hw_params with full configuration space
    if (snd_pcm_hw_params_any(pcm_handle, hw_params) < 0)
    {
        LUtil::error("Audio", "Failed to configure PCM device");
        return false;
    }

    // Set access type
    if (snd_pcm_hw_params_set_access(pcm_handle, hw_params, access) < 0)
    {
        LUtil::error("Audio", "Failed to set access to PCM device");
        return false;
    }

    // Set sample format
    if (snd_pcm_hw_params_set_format(pcm_handle, hw_params, format) < 0)
    {
        LUtil::error("Audio", "Failed to set sample format for PCM device");
        return false;
    }

    // Set sample rate
    exact_rate = config.sample_rate;
    if (snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &exact_rate, &dir) < 0)
    {
        LUtil::error("Audio", "Failed to set sample rate for PCM device");
        return false;
    }

    // If specified rate is not available, set to nearest rate
    if (config.sample_rate != static_cast<int>(exact_rate))
    {
        LUtil::error("Audio", "The sample rate " + std::to_string(config.sample_rate) + " is not supported. Using " + to_string(exact_rate) + " instead");
    }

    // Set number of channels
    if (snd_pcm_hw_params_set_channels(pcm_handle, hw_params, config.m_channels * config.n_channels) < 0)
    {
        LUtil::error("Audio", "Failed to set the numer of channels for the PCM device");
        return false;
    }

    // Set number of periods
    if (snd_pcm_hw_params_set_periods(pcm_handle, hw_params, periods, 0) < 0)
    {
        LUtil::error("Audio", "Failed to set the number of periods for the PCM device");
        return false;
    }

    // Set buffer size
    if (snd_pcm_hw_params_set_buffer_size(pcm_handle, hw_params, buffer_size) < 0)
    {
        LUtil::error("Audio", "Failed to set the buffer size for the PCM device");
        return false;
    }

    if (snd_pcm_hw_params(pcm_handle, hw_params) < 0)
    {
        LUtil::error("Audio", "Failed to set the hardware parameters for the PCM device");
        return false;
    }

    std::cout << "Audio: Finished initializing ALSA\n";

    return true;
} // initALSA

// Initialize AudioFile
bool Audio::initAudioFile()
{
    if (!audio_stream.load(config.recordings_dir + config.wav_file_name))
    {
        LUtil::error("Audio", "Failed to load the audio file " + config.wav_file_name);
        return false;
    }
    std::cout << "Audio: " << config.recordings_dir + config.wav_file_name << "\n";

    // Read the details of the file. May want to use these to compare to settings
    config.sample_rate = audio_stream.getSampleRate();
    std::cout << "Audio: Sample rate: " << audio_stream.getSampleRate() << "\n";
    std::cout << "Audio: Bit depth: " << audio_stream.getBitDepth() << "\n";
    std::cout << "Audio: Number of channels: " << audio_stream.getNumChannels() << "\n";
    std::cout << "Audio: Number of samples per channel: " << audio_stream.getNumSamplesPerChannel() << "\n";
    std::cout << "Audio: Length (seconds): " << audio_stream.getLengthInSeconds() << "\n";

    std::cout << "Audio: Finished initializing AudioFile\n";

    return true;
} // end initAudioFile

// Initialize either ALSA or AudioFile
bool Audio::initAudio()
{
    // Read from configs
    config.read();

    // Remap channel order
    channel_order = LUtil::unpackChannelOrder(config.channel_order);
    std::cout << "Mic Order:\n";
    channel_order.print();

    // Clear buffers
    read_buffer.fill(0.0f);
    write_buffer.fill(0.0f);

    std::string audio_backend = "ALSA";
    if (!config.use_alsa)
        audio_backend = "AudioFile";

    std::cout << "Audio: Initializing " << audio_backend << " audio backend\n";

    // Initialize desired audio backend
    if (config.use_alsa)
    {
        if (!initALSA())
        {
            LUtil::error("Audio", "Failed to initialize ALSA");
            return false;
        }
    }

    else 
    {
        if (!initAudioFile())
        {
            LUtil::error("Audio", "Failed to initialize AudioFile");
            return false;
        }
    }
 
    return true;
} // end initAudio

//=====================================================================================

/* Start and stop audio stream in a separate thread */

// Dispatch a thread and start audio stream
void Audio::startAudioStream()
{
    // Set streaming flag
    is_streaming = true;
    config.audio_is_streaming = is_streaming;

    // Dispatch a thread and start streaming audio from ALSA
    if (config.use_alsa)
        streaming_thread = std::thread(&Audio::streamAudioALSA, this);

    // Dispatch a thread and start streaming audio from a file
    else
        streaming_thread = std::thread(&Audio::streamAudioFile, this);
    
} // end startAudioStream

// Merge the audio thread and stop audio stream
void Audio::stopAudioStream()
{
    // Set streaming flag
    is_streaming = false;
    config.audio_is_streaming = is_streaming;

    // Merge thread
    if (streaming_thread.joinable())
        streaming_thread.join();

} // end stopAudio

//=====================================================================================

/* Stream audio with ALSA or AudioFile */

// Stream audio with ALSA
void Audio::streamAudioALSA()
{
    while (config.audio_is_streaming)
    {
        // Swap data from buffer_2 to buffer_1
        swap(read_buffer.data, write_buffer.data);

        // Read data from microphones into interlaced buffer
        pcm_return = snd_pcm_readi(pcm_handle, input_buffer, frames);
    
        // Check for error
        if (pcm_return == -EPIPE) 
        { 
            // Buffer overrun/underrun error
            pcm_error = 1;
            LUtil::error("Audio", "Buffer overrun/underrun occured. Recovering...");
            snd_pcm_prepare(pcm_handle); // Prepare the device again
            break;
        } 
        else if (pcm_return < 0) 
        {
            pcm_error = 2;
            std::cerr << "Audio Error: Reading PCM data: " << snd_strerror(pcm_return) << "\n";
            break;
        } 
        else if (pcm_return != static_cast<int>(frames)) 
        {
            pcm_error = 3;
            LUtil::error("Audio", "PCM read returned fewer frames than expected");
            break;
        }

        else if (pcm_return == static_cast<int>(frames))
        {
            pcm_error = 0;
        }

        // Remap the data to not-interlaced floats and normalize (-1, 1)
        for (size_t m = 0; m < write_buffer.dim_1; m++)
        {
            for (size_t n = 0; n < write_buffer.dim_2; n++)
            {
                for (size_t b = 0; b < write_buffer.dim_3; b++)
                {
                    write_buffer.at(m, n, b) = static_cast<float>(input_buffer[b * config.m_channels * config.n_channels + channel_order.at(m, n)]) / static_cast<float>(1 << 31);
                } // end m
            } // end n
        } // end b
    } // end loop
} // end streamAudioALSA

// Stream audio with AudioFile
void Audio::streamAudioFile()
{
    std::cout << "Audio: Starting AudioFile stream\n";
    int frame_counter = 0;
    int time_align_delay_micro = static_cast<int>(1e6 * static_cast<float>(config.fft_frame_size) 
                                                   / static_cast<float>(audio_stream.getSampleRate()));

    while (is_streaming)
    {
        // Wait until previous frame has been consumed
        {
            std::unique_lock<std::mutex> lock(audio_mutex);
            audio_cv.wait(lock, [this]{ return !frame_ready; });
        }

        // Fill write_buffer with new audio data
        for (size_t m = 0; m < write_buffer.dim_1; m++)
        {
            for (size_t n = 0; n < write_buffer.dim_2; n++)
            {
                for (size_t b = 0; b < write_buffer.dim_3; b++)
                {
                    int sample_index = frame_counter * config.fft_frame_size + b;
                    if (sample_index >= audio_stream.getNumSamplesPerChannel())
                        sample_index = 0; // Loop file

                    write_buffer.at(m, n, b) =
                        audio_stream.samples[channel_order.at(m, n)][sample_index];
                }
            }
        }

        frame_counter++;
        if (frame_counter * config.fft_frame_size >= audio_stream.getNumSamplesPerChannel())
        {
            frame_counter = 0;
            std::cout << "Repeating WAV file...\n";
        }

        // Swap buffers safely and mark frame as ready
        {
            std::lock_guard<std::mutex> lock(audio_mutex);
            swap(read_buffer.data, write_buffer.data);
            frame_ready = true;
        }
        audio_cv.notify_one(); // Notify consumer

        // Delay to mimic real-time playback
        std::this_thread::sleep_for(std::chrono::microseconds(time_align_delay_micro));
    }

    std::cout << "Audio: Ending AudioFile stream\n";
} // end streamAudioFile

bool Audio::getNextFrame(array3D<float>& out_buffer)
{
    std::unique_lock<std::mutex> lock(audio_mutex);

    // Wait until a frame is ready
    audio_cv.wait(lock, [this]{ return frame_ready || !is_streaming; });

    if (!frame_ready)
        return false; // Stream ended

    // Copy the data safely
    out_buffer = read_buffer;

    // Mark frame as consumed
    frame_ready = false;

    // Notify producer it can write next frame
    audio_cv.notify_one();

    return true;
}

//=====================================================================================

/* Record audio and audio data */

// Exports one channel from exisiting wav file
void Audio::exportFromWavFile(const int channel)
{
    if (channel >= audio_stream.getNumChannels() || channel < 0)
        LUtil::error("Audio", "Invalid channel selection (" + std::to_string(channel) + ")");

    // Create Audiofile for output and set parameters
    AudioFile<float> output;
    output.setBitDepth(audio_stream.getBitDepth());
    output.setNumChannels(1);
    output.setSampleRate(audio_stream.getSampleRate());
    output.setNumSamplesPerChannel(audio_stream.getNumSamplesPerChannel());

    // Copy samples from audio stream
    for (int i = 0; i < audio_stream.getNumSamplesPerChannel(); i++)
        output.samples[0][i] = audio_stream.samples[channel][i];

    if (!output.save("export.wav", AudioFileFormat::Wave))
        LUtil::error("Audio", "Failed to export audio to file");

    else
        std::cout << "Audio exported to file\n";

} // end exportFromWavFile