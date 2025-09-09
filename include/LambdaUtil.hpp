#pragma once

// Libraries
#include <iostream>
#include <string>
#include <math.h>
#include <algorithm>

// Headers
#include "Structs.hpp"

namespace LUtil
{
    // Pack and unpack channel order into a string
    std::string packChannelOrder(array2D<int>& unpacked_order);
    array2D<int> unpackChannelOrder(const std::string& packed_order);

    // Generate a radial gradient and move it around smoothly
    void radialGradient(array2D<float>& data, float radius, float speed, float& time);

    // Print an error message to the console
    void error(std::string error_name, std::string error_message); // TODO: Log to a file too
}