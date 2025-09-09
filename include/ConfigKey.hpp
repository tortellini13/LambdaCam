#pragma once

#include <cstdint>

// Enum definitions
namespace LKey
    {
    enum iConfig: uint8_t
    {
        M_CHANNELS,
        N_CHANNELS,
        FFT_FRAME_SIZE,
        SAMPLE_RATE,
        FOV_THETA,
        FOV_PHI,
        ANGLE_RESOLUTION,
        NUM_INT_CONFIGS
    };

    enum fConfig: uint8_t
    {
        MIC_SPACING_M,
        NUM_FLOAT_CONFIGS
    };

    enum bConfig: uint8_t
    {
        USE_ALSA,
        AUDIO_IS_STREAMING,
        NUM_BOOL_CONFIGS
    };

    enum sConfig: uint8_t
    {
        CHANNEL_ORDER,
        RECORDINGS_DIR,
        WAV_FILE_NAME,
        NUM_STRING_CONFIGS
    };

    // Name arrays
    constexpr const char* int_config_name[NUM_INT_CONFIGS] = 
    {
        "M Channels",
        "N Channels",
        "FFT Frame Size",
        "Sample Rate",
        "FOV Theta",
        "FOV Phi",
        "Angle Resolution"
    };

    constexpr const char* float_config_name[NUM_FLOAT_CONFIGS] = 
    {
        "Mic Spacing (m)"
    };

    constexpr const char* bool_config_name[NUM_BOOL_CONFIGS] = 
    {
        "Use ALSA",
        "Audio is Streaming"
    };

    constexpr const char* string_config_name[NUM_STRING_CONFIGS] = 
    {
        "Channel Order",
        "Recordings Directory",
        "Wav File Name"
    };
}