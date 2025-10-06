#include "LambdaUtil.hpp"

//=====================================================================================

// Pack channel order into a string with bit packing (15x15 max)
std::string LUtil::packChannelOrder(array2D<int>& unpacked_order)
{
    // Find the number of bits needed to represent the max number
    int num_channels = unpacked_order.dim_1 * unpacked_order.dim_2;
    int bits_per_channel = static_cast<int>(std::floor(std::log2(num_channels - 1))) + 1;

    // Total bytes required to store all bits
    int total_bytes = static_cast<int>((bits_per_channel * (num_channels) + 7) / 8);
    
    // Allocate memory for string and reserve the first byte for size information
    std::string packed_order(total_bytes + 1, '\0'); 

    // Set the first byte to indicate the number of channels
    packed_order[0] |= unpacked_order.dim_1 << 4; // Store dim_1 in the upper 4 bits of the first byte
    packed_order[0] |= unpacked_order.dim_2;      // Store dim_2 in the lower 4 bits of the first byte
    
    // Pack bits into a string
    int channel_index = 0;
    
    for (size_t m = 0; m < unpacked_order.dim_1; m++)
    {
        for (size_t n = 0; n < unpacked_order.dim_2; n++)
        {
            // Cache the channel number
            int channel_number = unpacked_order.at(m, n);

            // Loop through each bit int channel and set it
            for (int b = 0; b < bits_per_channel; b++)
            {
                // Reverse order for MSB first
                int reversed_bit = bits_per_channel - 1 - b;

                // Global bit index
                int bit_index = channel_index * bits_per_channel + b + 8; // +8 to skip the channel size byte

                // Local byte index and bit offset
                int current_byte = bit_index / 8;
                int bit_offset = 7 - (bit_index % 8); // Reverse order for MSB first
                
                // Check if the bit is set in the channel number, if so, set the bit in the packed string
                if (channel_number & (1 << reversed_bit))
                    packed_order[current_byte] |= (1 << bit_offset);
            }
            channel_index++;
        }
    }

    // Convert the packed string to a hex string for storage
    std::ostringstream oss;
    for (unsigned char c : packed_order)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    return oss.str();
} // end packChannelOrder

// Unpack channel order into an array2D (15x15 max)
array2D<int> LUtil::unpackChannelOrder(const std::string& packed_order)
{
    // Decode the string back to bytes
    std::string decoded_packed_order;
    decoded_packed_order.reserve(packed_order.size() / 2);

    for (std::size_t i = 0; i < packed_order.size(); i += 2)
    {
        std::string byteString = packed_order.substr(i, 2);
        char byte = static_cast<char>(std::stoi(byteString, nullptr, 16));
        decoded_packed_order.push_back(byte);
    }

    // Convert hex string to bytes
    // decoded_packed_order = hexStringToString(packed_order); // Convert hex string to bytes

    // Decode the array size from the first byte
    int m_channels = (decoded_packed_order[0] >> 4) & 0x0F; // Upper 4 bits for dim_1
    int n_channels = decoded_packed_order[0] & 0x0F;        // Lower 4 bits for dim_2

    // Find the number of bits needed to represent the max number
    int bits_per_channel = static_cast<int>(std::floor(std::log2(m_channels * n_channels - 1))) + 1;

    // Unpack bits from the string into the array
    array2D<int> unpacked_order(m_channels, n_channels);
    int channel_index = 0;

    for (int m = 0; m < m_channels; m++)
    {
        for (int n = 0; n < n_channels; n++)
        {
            // Loop through each bit in the channel
            int channel_number = 0;
            for (int b = 0; b < bits_per_channel; b++)
            {
                // Reverse order for MSB first
                int reversed_bit = bits_per_channel - 1 - b;

                // Global bit index
                int bit_index = channel_index * bits_per_channel + b + 8; // +8 to skip the channel size byte

                // Local byte index and bit offset
                int current_byte = bit_index / 8;
                int bit_offset = 7 - (bit_index % 8); // Reverse order for MSB first

                // Check if the bit is set in the packed string
                if (decoded_packed_order[current_byte] & (1 << bit_offset))
                    channel_number |= (1 << reversed_bit);
            }
            unpacked_order.at(m, n) = channel_number;
            channel_index++;
        }
    }
    return unpacked_order;
} // end unpackChannelOrder

//=====================================================================================

// Generate a radial gradient and move it around smoothly
void LUtil::radialGradient(array2D<float>& data_array, const float min, const float max, float& t)
{
    const float speed = 0.05f;  // Controls how fast the center moves
    const float radius = static_cast<int>(data_array.dim_1) * 0.1f; // Radius of the circular motion

    float center_x = data_array.dim_1 / 2.0f + radius * std::sin(t * speed);
    float center_y = data_array.dim_2 / 2.0f + radius * std::cos(t * speed);

    float max_radius = std::min(data_array.dim_1, data_array.dim_2) / 2.0f;

    for (size_t x = 0; x < data_array.dim_1; x++)
    {
        for (size_t y = 0; y < data_array.dim_2; y++)
        {
            float dx = static_cast<float>(x) - center_x;
            float dy = static_cast<float>(y) - center_y;
            float distance_from_center = std::sqrt(dx * dx + dy * dy);

            // Normalize and clamp value
            float norm = std::clamp(1.0f - distance_from_center / max_radius, 0.0f, 1.0f);
            float value = min + norm * (max - min);

            data_array.at(x, y) = value;
        }
    }

    t += 1.0f; // Increment time smoothly
} // end radialGradient

//=====================================================================================

// Print an error message to the console
void LUtil::error(std::string error_name, std::string error_message)
{
    std::cerr << "Error [" << error_name << "]: " << error_message << "\n";
} //end error

//=====================================================================================
