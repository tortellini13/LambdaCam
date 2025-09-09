// Libraries
#include <iostream> 
#include <alsa/asoundlib.h> 

// Headers
#include "Audio.hpp"
#include "Structs.hpp"
#include "LambdaUtil.hpp"
#include "ConfigIO.hpp"
#include "ConfigKey.hpp" 
#include "AudioFile.h"

//=====================================================================================

/* Class constructors and destructors */

Audio::Audio(CONFIG& global_config) :
    config(global_config), // Reference to the global configuration object

    m_channels(global_config.i(LKey::M_CHANNELS)), // Number of microphones in the m direction
    n_channels(global_config.i(LKey::N_CHANNELS)), // Number of microphones in the
    channel_order(m_channels, n_channels),  // Physical channel order to remap

    data_buffer_1(m_channels, n_channels, global_config.i(LKey::FFT_FRAME_SIZE)), // Buffer for audio data
    data_buffer_2(m_channels, n_channels, global_config.i(LKey::FFT_FRAME_SIZE)), // Buffer for audio data

    pcm_name("hw:0,0"),                            // Default ALSA device
    frames(global_config.i(LKey::FFT_FRAME_SIZE)), // Number of frames per period
    buffer_size(m_channels * n_channels * frames)  // Size of buffer from ALSA

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
    exact_rate = config.i(LKey::SAMPLE_RATE);
    if (snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &exact_rate, &dir) < 0)
    {
        LUtil::error("Audio", "Failed to set sample rate for PCM device");
        return false;
    }

    // If specified rate is not available, set to nearest rate
    if (config.i(LKey::SAMPLE_RATE) != static_cast<int>(exact_rate))
    {
        LUtil::error("Audio", "The sample rate " + std::to_string(config.i(LKey::SAMPLE_RATE)) + " is not supported. Using " + to_string(exact_rate) + " instead");
    }

    // Set number of channels
    if (snd_pcm_hw_params_set_channels(pcm_handle, hw_params, config.i(LKey::M_CHANNELS) * config.i(LKey::N_CHANNELS)) < 0)
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
    if (!audio_stream.load(config.s(LKey::RECORDINGS_DIR) + config.s(LKey::WAV_FILE_NAME)))
    {
        LUtil::error("Audio", "Failed to load the audio file " + config.s(LKey::WAV_FILE_NAME));
        return false;
    }

    // Read the details of the file. May want to use these to compare to settings
    config.i(LKey::SAMPLE_RATE) = audio_stream.getSampleRate();

    std::cout << "Audio: Finished initializing AudioFile\n";

    return true;
} // end initAudioFile

// Initialize either ALSA or AudioFile
bool Audio::initAudio()
{
    // Read from configs
    ConfigIO::readConfig(config);

    // Remap channel order
    channel_order = LUtil::unpackChannelOrder(config.s(LKey::CHANNEL_ORDER));

    // Clear buffers
    data_buffer_1.fill(0.0f);
    data_buffer_2.fill(0.0f);

    std::string audio_backend = "ALSA";
    if (!config.b(LKey::USE_ALSA))
        audio_backend = "AudioFile";

    std::cout << "Audio: Initializing " << audio_backend << " audio backend\n";

    // Initialize desired audio backend
    if (config.b(LKey::USE_ALSA))
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
    config.b(LKey::AUDIO_IS_STREAMING) = is_streaming;

    // Dispatch a thread and start streaming audio from ALSA
    if (config.b(LKey::USE_ALSA))
        streaming_thread = std::thread(&Audio::streamAudioALSA, this);

    // Dispatch a thread and start streaming audio from a file
    else
        streaming_thread = std::thread(&Audio::streamAudioFile, this);
    
} // end startAudio

// Merge the audio thread and stop audio stream
void Audio::stopAudioStream()
{
    // Set streaming flag
    is_streaming = false;
    config.b(LKey::AUDIO_IS_STREAMING) = is_streaming;

    // Merge thread
    if (streaming_thread.joinable())
        streaming_thread.join();

} // end stopAudio

//=====================================================================================

/* Stream audio with ALSA or AudioFile */

// Stream audio with ALSA
void Audio::streamAudioALSA()
{
    while (config.b(LKey::AUDIO_IS_STREAMING))
    {
        // Swap data from buffer_2 to buffer_1
        swap(data_buffer_1.data, data_buffer_2.data);

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
        for (size_t m = 0; m < data_buffer_2.dim_1; m++)
        {
            for (size_t n = 0; n < data_buffer_2.dim_2; n++)
            {
                for (size_t b = 0; b < data_buffer_2.dim_3; b++)
                {
                    data_buffer_2.at(m, n, b) = static_cast<float>(input_buffer[b * m_channels * n_channels + channel_order.at(m, n)]) / static_cast<float>(1 << 31);
                } // end m
            } // end n
        } // end b
    } // end loop
} // end streamAudioALSA

// Stream audio with AudioFile
void Audio::streamAudioFile()
{
    int stream_frame_counter = 0; // Frame counter for audio stream
    while (config.b(LKey::AUDIO_IS_STREAMING))
    {
        swap(data_buffer_1.data, data_buffer_2.data);

        // Loops wav file when it reaches the end
        if ((stream_frame_counter + data_buffer_2.dim_3) >= audio_stream.samples[0].size())
        {
            stream_frame_counter = 0;
            std::cout << "Audio: Repeating Wav File...\n";
        }

        for (size_t m = 0; m < data_buffer_2.dim_1; m++)
        {
            for (size_t n = 0; n < data_buffer_2.dim_2; n++)
            {
                for (size_t b = 0; b < data_buffer_2.dim_3; b++)
                {
                    data_buffer_2.at(m, n, b) =
                        audio_stream.samples[channel_order.at(m, n)][stream_frame_counter + b];
                }
            }
        }

        stream_frame_counter += data_buffer_2.dim_3;

        float time_align_delay = static_cast<float>(data_buffer_2.dim_3) / static_cast<float>(config.i(LKey::SAMPLE_RATE));
        std::this_thread::sleep_for(std::chrono::duration<float>(time_align_delay));
    }
} // end streamAudioFile

//=====================================================================================

/* Copy ring buffers to main thread */

// Access ring buffer
void Audio::accessRingBuffer(array3D<float>& data_output_1, array3D<float>& data_output_2)
{
    data_output_1 = data_buffer_1;
    data_output_2 = data_buffer_2;
} // end accessRingBuffer