#pragma once

#include "Structs.hpp"
#include "Config.hpp"
#include <fftw3.h>

class Beamform
{
public:
    // Constructor and destructor
    Beamform(Config &global_config);
    ~Beamform();

    // Initializes beamforming constants and FFT
    bool initBeamform();

    // Performs beamforming, FFT, and audio post-processing
    void processAudioFrame(array3D<float> &data_input, array2D<float>& data_output, const int frequency_bin);

private:
    // Initialize FFT and beamforming constants
    bool initFFT();
    bool initDirectivity();

    // Beamforming functions
    void applyBeamforming(array3D<float> &data_input, const int frequency_bin);
    void performFFT();

    // Helper function
    float degtorad(float angle_deg) const; // Converts degrees to radians

    // Global configuration object
    Config &config; // Reference to the global configuration object

    // FFT variables
    fftwf_plan fft_plan;              // FFT plan for reusing
    fftwf_complex *fft_input_buffer;  // 1D buffer for FFT input
    fftwf_complex *fft_output_buffer; // 1D buffer for FFT output
    float *hamming_weights;           // Hamming window weights

    // Arrays for beamforming
    array5D<complex<float>> directivity_factor; // (theta, phi, m, n, bin)
    array3D<complex<float>> data_beamform;      // (theta, phi, buffer)
    array3D<float> data_fft;                    // (theta, phi, bin)
};