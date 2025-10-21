// Libraries
#include <fftw3.h>   

// Headers
#include "Beamform.hpp"
#include "Structs.hpp"
#include "LambdaUtil.hpp"
#include "Config.hpp"

//=====================================================================================

Beamform::Beamform(Config &global_config) : 
    config(global_config) // Reference to the global configuration object
{
    
} // end Beamform

Beamform::~Beamform()
{

} // end ~Beamform

//=====================================================================================

float Beamform::degtorad(float angle_deg) const
{
    return angle_deg * (M_PI / 180.0f); // Convert degrees to radians
} // end degtorad

//=====================================================================================

bool Beamform::initFFT()
{
    // Allocate all arrays
    fft_input_buffer  = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * config.fft_frame_size); // Allocate buffer for FFT input
    fft_output_buffer = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * config.fft_frame_size); // Allocate buffer for FFT output

    // Create FFT plan
    fft_plan = fftwf_plan_dft_1d(config.fft_frame_size, fft_input_buffer, fft_output_buffer, FFTW_FORWARD, FFTW_ESTIMATE);

    return true;
} // end initFFT

bool Beamform::initDirectivity()
{
    int min_theta = -config.fov_theta / 2;
    int min_phi   = -config.fov_phi / 2;

    for (int theta = min_theta, t = 0; t < (int)directivity_factor.dim_1; theta += config.angle_resolution, t++)
    {
        for (int phi = min_phi, p = 0; p < (int)directivity_factor.dim_2; phi += config.angle_resolution, p++)
        {
            for (int m = 0; m < (int)directivity_factor.dim_3; m++)
            {
                for (int n = 0; n < (int)directivity_factor.dim_4; n++)
                {
                    for (int bin = 0; bin < (int)directivity_factor.dim_5; bin++)
                    {
                        float frequency = (config.sample_rate * bin) / config.fft_frame_size;
                        float wave_number = (2.0f * M_PI * frequency) / 343.0f;
                        float exponent = wave_number * config.mic_spacing * (m * sinf(degtorad(theta)) + n * sinf(degtorad(phi)));
                        float real = cosf(exponent);
                        float imag = -sinf(exponent);
                        directivity_factor.at(t, p, m, n, bin) = std::complex<float>(real, imag);
                    }
                }
            }
        }
    }

    return true;
} // end initDirectivity

bool Beamform::initBeamform()
{
    config.read();

    // Allocate memory for arrays
    int num_theta = (config.fov_theta / config.angle_resolution) + 1; // +1 to include both ends
    int num_phi   = (config.fov_phi / config.angle_resolution) + 1;   // +1 to include both ends

    hamming_weights = new float[config.fft_frame_size];
    directivity_factor.resize(num_theta, num_phi, config.m_channels, config.n_channels, config.fft_frame_size);
    data_beamform.resize(num_theta, num_phi, config.fft_frame_size);
    data_fft.resize(num_theta, num_phi, config.fft_frame_size);
    output_buffer.resize(num_theta, num_phi);

    // Clear arrays
    directivity_factor.fill(complex<float>(0.0f, 0.0f));
    data_beamform.fill(complex<float>(0.0f, 0.0f));
    data_fft.fill(0.0f);
    output_buffer.fill(0.0f);

    // Setup Hamming window
    for (int b = 0; b < config.fft_frame_size; b++)
    {
        float a0 = (25.0f / 46.0f); // Magic numbers
        hamming_weights[b] = a0 - (1.0f - a0) * cosf((2 * M_PI * static_cast<float>(b)) / static_cast<float>(config.fft_frame_size - 1));
    }

    // Initialize FFT and directivity factor
    if (!initFFT())
    {
        LUtil::error("Beamform", "Failed to initialize FFT");
        return false;
    }

    if (!initDirectivity())
    {
        LUtil::error("Beamform", "Failed to initialize directivity");
        return false;
    }

    std::cout << "Beamform: Finished initializing Beamform\n";

    return true;
} // end initBeamform

//=====================================================================================

void Beamform::applyBeamforming(array3D<float> &data_input, const int frequency_bin)
{
    // Parallelize theta, phi, and b. All are independant
    // *****TODO

    // Sum correlated microphone signals for each angle pair
    for (size_t theta = 0; theta < data_beamform.dim_1; theta++)
    {
        for (size_t phi = 0; phi < data_beamform.dim_2; phi++)
        {
            for (size_t b = 0; b < data_input.dim_3; b++)
            {
                complex<float> sum = complex<float>(0.0f, 0.0f);
                for (size_t m = 0; m < data_input.dim_1; m++)
                {
                    for (size_t n = 0; n < data_input.dim_2; n++)
                    {
                        // Convolve input signal with directivity factor
                        sum += data_input.at(m, n, b) * directivity_factor.at(theta, phi, m, n, frequency_bin);
                    }
                }
                // Write accumulated sum to the output
                data_beamform.at(theta, phi, b) = sum;
            }
        }
    } 
} // end applyBeamforming

//=====================================================================================

void Beamform::performFFT()
{
    // Iterate through each angle and perform many 1D FFTs
    for (size_t theta = 0; theta < data_beamform.dim_1; theta++)
    {
        for (size_t phi = 0; phi < data_beamform.dim_2; phi++)
        {
            // Write data to 1D buffer
            for (size_t b = 0; b < data_beamform.dim_3; b++)
            {
                // Apply Hamming window then extract real and imaginary parts
                std::complex<float> sample = data_beamform.at(theta, phi, b);
                fft_input_buffer[b][0] = real(sample) * hamming_weights[b];
                fft_input_buffer[b][1] = imag(sample) * hamming_weights[b];
            }

            // Execute fft plan on buffer
            fftwf_execute(fft_plan);

            // Convert to dBfs
            for (size_t bin = 0; bin < data_fft.dim_3; bin++)
            {
                // Normalize output
                float real = fft_output_buffer[bin][0] / config.fft_frame_size;
                float imag = fft_output_buffer[bin][1] / config.fft_frame_size;

                // 20 * log10(signal)
                float arg = real * real + imag * imag;
                data_fft.at(theta, phi, bin) = 20 * log10f(sqrt(arg));
            }
        }
    }
} // end performFFT

//=====================================================================================

void Beamform::processAudioFrame(array3D<float> &data_input, const int frequency_bin)
{
    // Beamform incoming data from mic array
    applyBeamforming(data_input, frequency_bin);

    // Perfrom FFT on beamformed data
    performFFT();

    // Write desired bin to output
    for (size_t theta = 0; theta < data_fft.dim_1; theta++)
    {
        for (size_t phi = 0; phi < data_fft.dim_2; phi++)
        {
            output_buffer.at(theta, phi) = data_fft.at(theta, phi, frequency_bin);
        }
    }
} // end processAudioFrame