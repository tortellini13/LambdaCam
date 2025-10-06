// Libraries
#include <iostream>
#include <fstream>

// Headers
#include "ConfigIO.hpp"
#include <nlohmann/json.hpp>
#include "ConfigKey.hpp"
#include "LambdaUtil.hpp"

namespace ConfigIO
{
    bool writeConfig(CONFIG& config)
    {
        nlohmann::json config_data = 
        {
            {"int_configs", nlohmann::json::object()},
            {"float_configs", nlohmann::json::object()},
            {"bool_configs", nlohmann::json::object()},
            {"string_configs", nlohmann::json::object()}
        };

        for (int i = 0; i < LKey::NUM_INT_CONFIGS; i++)    {config_data["int_configs"][LKey::int_config_name[i]] = config.i(i);}
        for (int i = 0; i < LKey::NUM_FLOAT_CONFIGS; i++)  {config_data["float_configs"][LKey::float_config_name[i]] = config.f(i);}
        for (int i = 0; i < LKey::NUM_BOOL_CONFIGS; i++)   {config_data["bool_configs"][LKey::bool_config_name[i]] = config.b(i);}
        for (int i = 0; i < LKey::NUM_STRING_CONFIGS; i++) {config_data["string_configs"][LKey::string_config_name[i]] = config.s(i);}

        std::ofstream outFile("config.json");
        if (!outFile.is_open()) 
        {
            LUtil::error("ConfigIO", "Failed to open the config file for writing");
            return false;
        }

        outFile << config_data.dump(4);
        outFile.close();

        std::cout << "Done writing configs\n";
        return true;
    } // end writeConfig

    bool readConfig(CONFIG& config, bool use_default)
    {
        // Open config file
        std::ifstream inFile;
        if (use_default)
            inFile.open("defaultConfig.json");
        else
            inFile.open("config.json");
            
        if (!inFile.is_open()) 
        {
            LUtil::error("ConfigIO", "No config file found. Creating one with default settings");
            writeConfig(config);
            inFile.open("config.json");
        }

        nlohmann::json config_data;
        inFile >> config_data;
        inFile.close();

        for (int i = 0; i < LKey::NUM_INT_CONFIGS; i++)    {config.i(i) = config_data["int_configs"][LKey::int_config_name[i]].get<int>();}
        for (int i = 0; i < LKey::NUM_FLOAT_CONFIGS; i++)  {config.f(i) = config_data["float_configs"][LKey::float_config_name[i]].get<float>();}
        for (int i = 0; i < LKey::NUM_BOOL_CONFIGS; i++)   {config.b(i) = config_data["bool_configs"][LKey::bool_config_name[i]].get<bool>();}
        for (int i = 0; i < LKey::NUM_STRING_CONFIGS; i++) {config.s(i) = config_data["string_configs"][LKey::string_config_name[i]].get<std::string>();}

        if (use_default)
            std::cout << "Done reading default configs\n";
        else
            std::cout << "Done reading configs\n";
        return true;
    } // end readConfig

    void print(CONFIG& config, int type)
    {   
        switch (type)
        {
            case TYPE::ALL:
            std::cout << "==================================================\n";
                for (size_t i = 0; i < config.i_size; i++)
                    std::cout << LKey::int_config_name[i] << ": " << config.i(i) << "\n";
                std::cout << "\n";

                for (size_t f = 0; f < config.f_size; f++)
                    std::cout << LKey::float_config_name[f] << ": " << config.f(f) << "\n";
                std::cout << "\n";

                for (size_t b = 0; b < config.b_size; b++)
                    std::cout << LKey::bool_config_name[b] << ": " << config.b(b) << "\n";
                std::cout << "\n";

                for (size_t s = 0; s < config.s_size; s++)
                    std::cout << LKey::string_config_name[s] << ": " << config.s(s) << "\n";
                std::cout << "==================================================\n";

            break;

            case TYPE::INT:
                for (size_t i = 0; i < config.i_size; i++)
                    std::cout << LKey::int_config_name[i] << ": " << config.i(i) << "\n";
            break;

            case TYPE::FLOAT:
                for (size_t f = 0; f < config.f_size; f++)
                    std::cout << LKey::float_config_name[f] << ": " << config.f(f) << "\n";
            break;

            case TYPE::BOOL:
                for (size_t b = 0; b < config.b_size; b++)
                    std::cout << LKey::bool_config_name[b] << ": " << config.b(b) << "\n";
            break;

            case TYPE::STRING:
                for (size_t s = 0; s < config.s_size; s++)
                    std::cout << LKey::string_config_name[s] << ": " << config.s(s) << "\n";
            break;

            default:
                LUtil::error("ConfigIO", "Invalid config type");
            break;
        }
    } // end print   
} 