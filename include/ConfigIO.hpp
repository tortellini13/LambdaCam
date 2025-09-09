#pragma once

#include "Structs.hpp"

namespace ConfigIO
{
    enum TYPE: int
    {
        ALL,
        INT,
        FLOAT,
        BOOL,
        STRING
    };

    bool writeConfig(CONFIG& config);
    bool readConfig(CONFIG& config, bool use_default = false);
    void clear(CONFIG& config);
    void print(CONFIG& config, int type = TYPE::ALL);
}
