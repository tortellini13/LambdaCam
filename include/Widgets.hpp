#pragma once

#include "imgui.h"
#include "string"

struct LSlider
{
    const char* label;
    float value = 0.0f;
    float min = 0.0f;
    float max = 100.0f;
    ImVec2 position{100, 100};
    ImVec2 size{50, 200};
    bool start = false;

    LSlider(const char* lbl, float mn, float mx) : label(lbl), min(mn), max(mx) {}

    void update(ImVec2 new_position, ImVec2 new_size, float& control)
    {
        if (!start)
        {
            value = control; // Initialize internal value on first run
            start = true;
        }
        
        // Update the position and size
        position = new_position;
        size = new_size;

        ImGui::SetCursorPos(position);
        ImGui::BeginGroup();

        // Create centered label
        ImVec2 text_size = ImGui::CalcTextSize(label); 
        float text_x = position.x + (size.x - text_size.x) * 0.5f; // Center horizontally
        ImGui::SetCursorPosX(text_x); 
        ImGui::Text("%s", label);

        // Create slider
        std::string id = "##" + std::string(label);
        ImGui::SetCursorPosX(position.x); // Reset to group left
        ImGui::VSliderFloat(id.c_str(), size, &value, min, max); // Create a vertical slider
        ImGui::EndGroup();                // End the group

        // Update the external control variable
        control = value;
    }
}; // end LSlider

struct LButton
{
    const char* label;
    bool is_held = false;
    bool is_triggered = false;
    ImVec2 position{100, 100};
    ImVec2 size{50, 50};

    LButton(const char* lbl) : label(lbl) {}

    void update(ImVec2 new_position, ImVec2 new_size)
    {
        // Update the position and size
        position = new_position;
        size = new_size;

        // Render button
        ImGui::SetCursorPos(position);
        is_triggered = ImGui::Button(label, size); // Check if button was pressed (falling edge)
        is_held = ImGui::IsItemActive();           // Check if the button is being held
    }
}; // end LButton

struct LCheckBox
{
    const char* label;
    bool state = false;
    ImVec2 position{100, 100};
    ImVec2 size{100, 50}; // width x height

    LCheckBox(const char* lbl) : label(lbl) {}

    void update(ImVec2 new_position, ImVec2 new_size)
    {
        // Update the position and size
        position = new_position;
        size = new_size;

        ImGui::SetCursorPos(position);
        ImGui::BeginGroup();

        // Compute padding so the checkbox scales with desired height
        ImGuiStyle& style = ImGui::GetStyle();
        float frame_padding_y = (size.y - ImGui::GetTextLineHeight()) * 0.5f;
        frame_padding_y = frame_padding_y > 0 ? frame_padding_y : 0;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, frame_padding_y));

        // Draw checkbox
        std::string id = "##" + std::string(label);
        ImGui::Checkbox(id.c_str(), &state);

        ImGui::PopStyleVar();

        // Center text vertically with checkbox
        ImVec2 text_size = ImGui::CalcTextSize(label);
        float text_x = position.x + (size.x - text_size.x) * 0.5f;
        float text_y = position.y + (size.y - text_size.y) * 0.5f;
        ImGui::SetCursorPos(ImVec2(text_x, text_y));
        ImGui::Text("%s", label);

        ImGui::EndGroup();
    }
}; // end LCheckBox

struct LDropDown
{
    const char* label;
    int current_item = 0;
    ImVector<const char*> items;
    ImVec2 position{100, 100};
    ImVec2 size{100, 25};

    LDropDown(const char* lbl, std::initializer_list<const char*> list)
        : label(lbl)
    {
        for (auto item : list)
            items.push_back(item);
    }

    // Method to update items later
    void setItems(std::initializer_list<const char*> list)
    {
        items.clear();
        for (auto item : list)
            items.push_back(item);
        current_item = 0; // reset selection
    }

    void update(ImVec2 new_position, ImVec2 new_size)
    {
        // Update the position and size
        position = new_position;
        size = new_size;

        ImGui::SetCursorPos(position);
        ImGui::BeginGroup();

        // Draw the label above the combo box
        ImGui::Text("%s", label);

        // Use a unique ID for the combo to avoid conflicts
        std::string id = "##" + std::string(label);

        // Set the width of the combo box
        ImGui::SetNextItemWidth(size.x);

        // Create the combo box
        if (ImGui::BeginCombo(id.c_str(), items[current_item]))
        // if (ImGui::BeginCombo(label, items[current_item]))
        {
            for (int i = 0; i < items.Size; i++)
            {
                bool is_selected = (current_item == i);
                if (ImGui::Selectable(items[i], is_selected))
                    current_item = i;

                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndGroup();
    }
}; // end LDropDown

struct LTextInput
{
    const char* label;
    char text[256] = {0};
    ImVec2 position{100, 100};
    ImVec2 size{100, 25};

    LTextInput(const char* lbl) : label(lbl) {}

    void update(ImVec2 new_position, ImVec2 new_size)
    {
        // Update the position and size
        position = new_position;
        size = new_size;

        ImGui::SetCursorPos(position);
        ImGui::BeginGroup();

        // Draw the label above the input box
        ImGui::Text("%s", label);

        // Use a unique ID for the input to avoid conflicts
        std::string id = "##" + std::string(label);

        // Set the width of the input box
        ImGui::SetNextItemWidth(size.x);

        // Create the input text box
        ImGui::InputText(id.c_str(), text, 256); // Assuming max length of 256 characters

        ImGui::EndGroup();
    }
}; // end LTextInput

struct FrequencySlider
{
    const char* label;
    float value = 1000.0f;
    float min = 0.0f;
    float max = 100.0f;
    ImVec2 position{100, 100};
    ImVec2 size{50, 200};

    FrequencySlider(const char* lbl, float mn, float mx) : label(lbl), min(mn), max(mx) {}

    void update(ImVec2 new_position, ImVec2 new_size)
    {
        // Update the position and size
        position = new_position;
        size = new_size;

        ImGui::SetCursorPos(position);
        ImGui::BeginGroup();

        // Create centered label
        // ImVec2 text_size = ImGui::CalcTextSize(label); 
        // float text_x = position.x + (size.x - text_size.x) * 0.5f; // Center horizontally
        // ImGui::SetCursorPosX(text_x); 
        // ImGui::Text("%s", label);

        // Create slider
        std::string id = "##" + std::string(label);
        ImGui::SetCursorPosX(position.x); // Reset to group left
        ImGui::SetNextItemWidth(size.x);  // Set the width of the slider
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, 
            ImVec2(0, (size.y - ImGui::GetFontSize()) / 2.0f)); // Set height by adding frame padding
        ImGui::SliderFloat(id.c_str(), &value, min, max);       // Create a vertical slider
        ImGui::PopStyleVar();             // Reset the style var to the default
        ImGui::EndGroup();                // End the group
    }
}; // end FrequencySlider

struct LLegend
{
    ImVec2 position{100, 100};
    ImVec2 size{50, 200};
    GLuint textureId = 0;  // OpenGL texture handle for the legend image

    LLegend() = default;

    // Optional constructor to set initial pos/size
    LLegend(ImVec2 pos, ImVec2 sz) : position(pos), size(sz) {}

    void setTexture(GLuint tex)
    {
        textureId = tex;
    }

    void update(ImVec2 new_position, ImVec2 new_size, float minVal, float maxVal)
    {
        position = new_position;
        size = new_size;

        // Set cursor to desired position
        ImGui::SetCursorPos(position);
        ImGui::BeginGroup();

        if (textureId != 0)
        {
            // Draw legend image
            ImGui::Image((ImTextureID)(intptr_t)textureId, size);

            // Min and max labels
            ImGui::SetCursorPos({position.x + size.x + 5, position.y}); // right of legend
            ImGui::Text("%.2f", maxVal);

            ImGui::SetCursorPos({position.x + size.x + 5, position.y + size.y - ImGui::GetTextLineHeight()});
            ImGui::Text("%.2f", minVal);
        }
        else
        {
            ImGui::Text("Legend (no texture)");
        }

        ImGui::EndGroup();
    }
}; // end LLegend