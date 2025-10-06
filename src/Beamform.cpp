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
    // Clear directovity factor array
    directivity_factor.fill(0.0f);

    // Cache values from config for ease of use
    int fov_theta        = config.fov_theta;
    int fov_phi          = config.fov_phi;
    int angle_resolution = config.angle_resolution;
    int m_channels       = config.m_channels;
    int n_channels       = config.n_channels;
    int fft_frame_size   = config.fft_frame_size;
    int sample_rate      = config.sample_rate;
    float mic_spacing_m  = config.mic_spacing;
    float speed_of_sound = 343.0f; // Speed of sound in air at 20 degrees Celsius in m/s

    int half_fov_theta = fov_theta / 2;
    int half_fov_phi   = fov_phi / 2;
    
    int num_theta = (fov_theta / angle_resolution) + 1; // +1 to include both ends
    int num_phi   = (fov_phi / angle_resolution) + 1;   // +1 to include both ends

    for (int theta_index = 0; theta_index < num_theta; theta_index++)
    {
        float theta = -half_fov_theta + theta_index * angle_resolution;
        for (int phi_index = 0; phi_index < num_phi; phi_index++)
        {
            float phi = -half_fov_phi + phi_index * angle_resolution;
            for (int m = 0; m < m_channels; m++)
            {
                for (int n = 0; n < n_channels; n++)
                {
                    for (int bin = 0; bin < fft_frame_size; bin++)
                    {
                        // Computes steering vector for beamforming
                        float frequency = static_cast<float>(sample_rate * bin) / static_cast<float>(fft_frame_size);
                        float wave_number = (2.0f * M_PI * frequency) / speed_of_sound;
                        float exponent = wave_number * mic_spacing_m * (static_cast<float>(m) * sinf(degtorad(theta)) + static_cast<float>(n) * sinf(degtorad(phi)));
                        float real = cosf(exponent);
                        float imag = -sinf(exponent);

                        // std::cout << "real: " << real << " | imag: " << imag << "\n";

                        // std::cout << "theta: " << directivity_factor.dim_1 << " | phi: " << directivity_factor.dim_2 << " | m: " << directivity_factor.dim_3 << " | n: " << directivity_factor.dim_4 << " | bin: " << directivity_factor.dim_5 << "\n";
                        // std::cout << "theta: " << num_theta << " | phi: " << num_phi << " | m: " << m_channels << " | n: " << n_channels << " | bin: " << fft_frame_size << "\n";
                        // std::cout << "theta: " << theta_index << " | phi: " << phi_index << " | m: " << m << " | n: " << n << " | bin: " << bin << "\n";
                        
                        directivity_factor.at(theta_index, phi_index, m, n, bin) = complex<float>(real, imag);

                        // std::cout << "Wrote directivity\n";
                    }
                } 
            }
        } 
    }

    return true;
} // end initDirectivity

bool Beamform::initBeamform()
{
    // Allocate memory for arrays
    int num_theta = (config.fov_theta / config.angle_resolution) + 1; // +1 to include both ends
    int num_phi   = (config.fov_phi / config.angle_resolution) + 1;   // +1 to include both ends

    hamming_weights = new float[config.fft_frame_size];
    directivity_factor.resize(num_theta, num_phi, config.m_channels, config.n_channels, config.fft_frame_size);
    data_beamform.resize(num_theta, num_phi, config.fft_frame_size);
    data_fft.resize(num_theta, num_phi, config.fft_frame_size);

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
                std::complex<float> sample = data_beamform.at(theta, phi, b) * hamming_weights[b];
                fft_input_buffer[b][0] = real(sample);
                fft_input_buffer[b][1] = imag(sample);
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

void Beamform::processAudioFrame(array3D<float> &data_input, array2D<float>& data_output, const int frequency_bin)
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
            data_output.at(theta, phi) = data_fft.at(theta, phi, frequency_bin);
        }
    }
} // end processAudioFrame