#pragma once

// Libraries
#include <iostream>
#include <complex>            // Complex numbers
#include <omp.h>              // OpenMP for parallelization
#include <fftw3.h>            // FFT  
#include <opencv2/opencv.hpp> // For outputting cv::Mat


// Headers
#include "PARAMS.h"
#include "Structs.h"
#include "Timer.h" 

class beamform
{
public:

    // Constructor
    beamform(int temp);

    // Destructor
    ~beamform();

    // Sets up all constants and initialized FFT
    void setup();

    // Performs beamforming
    void processData(cv::Mat &data_output, const int frequency_bin, array3D<float> &data_input);

private:
    // Setup functions
    void setupDirectivity(); // Computes directivity factor for phased array
    void setupFFT();         // Creates FFT plan

    // Beamforming functions
    void handleBeamforming(array3D<float> &data_input, const int frequency_bin); // Multiplies mic signal by steering vector and accumulates
    void FFT();                                // Performs FFT on beamformed signal
    void FFTCollapse(const int frequency_bin); // Returns the desired FFT bin

    // Helper functions
    float degtorad(const float angle_deg);            // Converts degrees to radians
    cv::Mat array2DtoMat(const array2D<float> &data); // Converts array2D to cv::Mat

    // FFT variables
    fftwf_plan fft_plan;              // Plan for fft to reuse
    fftwf_complex *fft_input_buffer;  // 1D buffer for input
    fftwf_complex *fft_output_buffer; // 1D buffer for output
    float hamming_weights[FFT_SIZE];  // Hamming window weights

    // Arrays
    array5D<complex<float>> directivity_factor; // (theta, phi, m, n, bin)
    array3D<complex<float>> data_beamform;      // (theta, phi, buffer) need to add bin dim later
    array3D<float> data_fft;                    // (theta, phi, bin)
    array2D<float> data_fft_collapse;           // (theta, phi)

    // Timers for profiling
    timer fft_time;
    timer beamform_time;
    timer fft_collapse_time;
    timer post_process_time;

    // Not used
    int temp;

}; // end class def

beamform::beamform(int temp):
    temp(temp),
    // Initialize timer names
    fft_time("FFT"),
    beamform_time("Beamform"),
    fft_collapse_time("FFT Collapse"),
    post_process_time("Post Process"),

    // Allocate memory to arrays
    directivity_factor(NUM_THETA, NUM_PHI, M_AMOUNT, N_AMOUNT, FFT_SIZE),
    data_beamform(NUM_THETA, NUM_PHI, FFT_SIZE),
    data_fft(NUM_THETA, NUM_PHI, FFT_SIZE),
    data_fft_collapse(NUM_THETA, NUM_PHI)
    {} // end beamform

// Destructor
beamform::~beamform()
{
    // Free FFTW plan
    fftwf_destroy_plan(fft_plan);
} // end ~beamform

//=====================================================================================

float beamform::degtorad(const float input_degrees)
{
    return input_degrees * M_PI / 180.0f;
} // end degtorad

cv::Mat beamform::array2DtoMat(const array2D<float> &data)
{
    // Create a Mat and reassign data
    cv::Mat mat(data.dim_1, data.dim_2, CV_32FC1, data.data);
    return mat;
} // end array2DtoMat

//=====================================================================================

// Setup functions

void beamform::setupDirectivity()
{
    // Precompute directivity factor for all angles
    for (int theta = MIN_THETA, theta_index = 0; theta_index < directivity_factor.dim_1; theta += STEP_THETA, theta_index++)
    {
        for (int phi = MIN_PHI, phi_index = 0; phi_index < directivity_factor.dim_2; phi += STEP_PHI, phi_index++)
        {
            for (int m = 0; m < directivity_factor.dim_3; m++)
            {
                for (int n = 0; n < directivity_factor.dim_4; n++)
                {
                    for (int bin = 0; bin < directivity_factor.dim_5; bin++)
                    {
                        // Computes steering vector for beamforming
                        float frequency = (SAMPLE_RATE * bin) / FFT_SIZE;
                        float wave_number = (2 * M_PI * frequency) / SPEED_OF_SOUND; 
                        float exponent = wave_number * MIC_SPACING * (m * sinf(degtorad(theta)) + n * sinf(degtorad(phi)));
                        float real = cosf(exponent);
                        float imag = -sinf(exponent);
                        directivity_factor.at(theta_index, phi_index, m, n, bin) = complex<float>(real, imag);
                    } // end b
                } // end n
            } // end m
        } // end phi
    } // end theta
} // end directivity

void beamform::setupFFT()
{
    // Allocate all arrays
    fft_input_buffer  = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE); // Allocate buffer for FFT input
    fft_output_buffer = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE); // Allocate buffer for FFT output

    // Create FFT plan
    fft_plan = fftwf_plan_dft_1d(FFT_SIZE, fft_input_buffer, fft_output_buffer, FFTW_FORWARD, FFTW_ESTIMATE);
} // end setupFFT

void beamform::setup()
{
    directivity_factor.fill(0.0f); // Initialize directivity factor to zero
    setupDirectivity();
    setupFFT();

    // Setup Hamming window
    for (int b = 0; b < FFT_SIZE; b++)
    {
        float a0 = (25.0f / 46.0f); // Magic numbers
        hamming_weights[b] = a0 - (1.0f - a0) * cosf((2 * M_PI * static_cast<float>(b)) / static_cast<float>(FFT_SIZE));
    } // end b

} // end setup

//=====================================================================================

void beamform::handleBeamforming(array3D<float> &data_input, const int frequency_bin)
{
    // Sum correlated microphone signals for each angle pair and frame
    #pragma omp parallel for collapse(3) // Parallelize theta, phi, and b
    for (int theta = 0; theta < data_beamform.dim_1; theta++)
    {
        for (int phi = 0; phi < data_beamform.dim_2; phi++)
        {
            for (int b = 0; b < data_input.dim_3; b++)
            {
                complex<float> sum = complex<float>(0.0f, 0.0f); // Reset sum
                for (int m = 0; m < data_input.dim_1; m++)
                {
                    for (int n = 0; n < data_input.dim_2; n++)
                    {
                        sum += data_input.at(m, n, b) * directivity_factor.at(theta, phi, m, n, frequency_bin); // Multiply by steering vector and accumulate
                    } // end n
                } // end m
                data_beamform.at(theta, phi, b) = sum; // Write accumulated sum to output
            } // end b
        } // end theta
    } // end theta
} // end handleBeamforming

//=====================================================================================

void beamform::FFT()
{
    // Loop through each angle and perform 1D FFT on each
    for (int theta = 0; theta < data_beamform.dim_1; theta++)
    {
        for (int phi = 0; phi < data_beamform.dim_2; phi++)
        {
            // Write data to input buffer
            for (int b = 0; b < data_beamform.dim_3; b++)
            {
                fft_input_buffer[b][0] = real(data_beamform.at(theta, phi, b)) * hamming_weights[b]; // Apply Hamming window
                fft_input_buffer[b][1] = imag(data_beamform.at(theta, phi, b)) * hamming_weights[b]; // Apply Hamming window
            } // end b

            // Call fft plan
            fftwf_execute(fft_plan);

            // Convert to dBfs
            for (int bin = 0; bin < data_fft.dim_3; bin++)
            {
                float real = fft_output_buffer[bin][0] / FFT_SIZE; // Normalize by FFT size
                float imag = fft_output_buffer[bin][1] / FFT_SIZE; // Normalize by FFT size
                float inside = real * real + imag * imag;
                data_fft.at(theta, phi, bin) = 20 * log10f(sqrt(inside));
            } // end b
        } // end n
    } // end m
} // end FFT

//=====================================================================================

void beamform::FFTCollapse(const int frequency_bin)
{
    // Return only the desired band. All others have not been beamformed
    for (int theta = 0; theta < data_fft.dim_1; theta++)
    {
        for (int phi = 0; phi < data_fft.dim_2; phi++)
        {
            data_fft_collapse.at(theta, phi) = data_fft.at(theta, phi, frequency_bin);
        } // end phi
    } // end theta
} // end FFTCollapse

//=====================================================================================

void beamform::processData(cv::Mat &data_output, const int frequency_bin, array3D<float> &data_input)
{
    // Beamform incoming data from mics
    beamform_time.start();
    handleBeamforming(data_input, frequency_bin);
    beamform_time.stop();

    // Perform FFT on beamformed data
    fft_time.start();
    FFT();
    fft_time.stop();

    // Collapse FFT data
    fft_collapse_time.start();
    FFTCollapse(frequency_bin);
    fft_collapse_time.stop();

    // Convert to cv::Mat
    data_output = array2DtoMat(data_fft_collapse);
    
    // Outputs timers to the console for debugging
    #ifdef PROFILE_BEAMFORM
    // Print profiling data
    beamform_time.print_avg(AVG_SAMPLES);
    fft_time.print_avg(AVG_SAMPLES);
    fft_collapse_time.print_avg(AVG_SAMPLES);
    if (beamform_time.getCurrentAvgCount() > AVG_SAMPLES - 1)
    {

        cout << "\n";
    }
    #endif
} // end processData