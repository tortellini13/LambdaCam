// Libraries
#include <iostream>
#include <cmath>
#include <chrono>

// Headers
#include "Structs.h"
#include "Timer.h"
#include "PARAMS.h"
#include "AudioFile.h"

using namespace std;

class WAV
{
public:
    // Constructor
    WAV();

    // Destructor
    ~WAV();

    // Sets up all constants and initialized FFT
    bool setup(const char* file_name);

    // Writes data to wav file
    void readWAV(array3D<float>& data_buffer_1, array3D<float>& data_buffer_2);

private:

timer WAV_timer;               // Timer for time travel!?
AudioFile<double> input_audio; // Input audio stream

int sample_rate;             // Sample rate from the file
int bit_depth;               // Bit depth from the file
int num_samples_per_channel; // Total number of samples per channel from file
double file_length;          // Length of file in seconds
int num_channels;            // Number of audio channels from file

int b_file; // What does this mean??

array2D<int> channel_order;
array3D<float> data_buffer_1;
array3D<float> data_buffer_2; 

}; // end class def

WAV::WAV():    
    // Initialize default values
    sample_rate(0),
    bit_depth(0),
    num_samples_per_channel(0),
    file_length(0.0),
    num_channels(0),
    b_file(0),

    // Alocate memory to arrays
    channel_order(M_AMOUNT, N_AMOUNT),
    data_buffer_1(M_AMOUNT, N_AMOUNT, FFT_SIZE),
    data_buffer_2(M_AMOUNT, N_AMOUNT, FFT_SIZE), 
    
    // Set timer name
    WAV_timer("WAV") 
    {} // end WAV

WAV::~WAV()
{

} // end ~WAV

//=====================================================================================

bool WAV::setup(const char* file_name) 
{
    // Map channel order
    for (int m = 0; m < channel_order.dim_1; m++)
    {
        for (int n = 0; n < channel_order.dim_2; n++)
        {
            channel_order.at(m, n) = CHANNEL_ORDER[m][n];
        }
    }
    
    // Load the audio file
    if (!input_audio.load(file_name)) 
    {
        cerr << "Error: Could not load audio file" << "\n";
        return false;
    }

    // Read details of the file
    sample_rate = input_audio.getSampleRate(); 
    bit_depth = input_audio.getBitDepth();
    num_samples_per_channel = input_audio.getNumSamplesPerChannel();
    file_length = input_audio.getLengthInSeconds();
    num_channels = input_audio.getNumChannels();

    // Print Details (Debugging)
    cout << "Sample Rate: " << sample_rate << "\n";
    cout << "Bit Depth: " << bit_depth << "\n";
    cout << "Number of Samples per Channel: " << num_samples_per_channel << "\n";
    cout << "Length in Seconds: " << file_length << "\n";
    cout << "Number of Channels: " << num_channels << "\n";

    // Check if the file matches current camera configuration
    if (num_channels != NUM_CHANNELS)
    {
        cerr << "Error: Number of channels in the file does not match the camera configuration" << "\n";
        return false;
    }
    if (sample_rate != SAMPLE_RATE)
    {
        cerr << "Error: Sample rate in the file does not match the camera configuration" << "\n";
        return false;
    }
    if (bit_depth != 32)
    {
        cerr << "Error: Bit depth in the file does not match the camera configuration" << "\n";
        return false;
    }
    cout << "File matches camera configuration" << "\n";
    
    // Start the timer
    WAV_timer.start(); 

    return true;
} // end setup

//=====================================================================================

void WAV::readWAV(array3D<float>& data_buffer_1, array3D<float>& data_buffer_2) 
{
    // Swap buffers 1 and 2
    swap(data_buffer_1.data, data_buffer_2.data);

    // Loops wav file when it reaches the end
    if((b_file * SAMPLE_RATE) + data_buffer_2.dim_3 > num_samples_per_channel)
    {
        b_file = 0;
        cout << "Repeating Wav File...\n";
    }

    // Read audio file into buffer
    for (int m = 0; m < data_buffer_2.dim_1; m++)
    {
        for (int n = 0; n < data_buffer_2.dim_2; n++)
        {
            for (int b = 0; b < data_buffer_2.dim_3; b++)
            {
                data_buffer_2.at(m, n, b) = input_audio.samples[channel_order.at(m, n)][b_file * 1024 + b];
            } // end b
        } // end n
    } // end m

    b_file++;

    // End the timer
    WAV_timer.stop(); 

    // Restart the timer
    WAV_timer.start(); 
} // end readWAV