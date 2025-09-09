# Compiler and flags
CXX := g++
CXX_FLAGS := -Wall -Wextra -Wno-strict-aliasing -std=c++17 -O2 -D_REENTRANT `pkg-config --cflags opencv4 sdl2`
INCLUDES := -Iinclude -Iimgui 

# Other Flags
OPENCV_FLAGS := `pkg-config --libs opencv4`
IMGUI_FLAGS  := -lSDL2 -lGL -ldl
ALSA_FLAGS   := -lasound
FFTW_FLAGS   := -lfftw3f -lm -lfftw3f_threads
FLAGS := $(OPENCV_FLAGS) $(IMGUI_FLAGS) $(ALSA_FLAGS) $(FFTW_FLAGS)

# Source and build directories
SRC_DIR := src
OBJ_DIR := build
BIN := LambdaCam

# Find all .cpp files
SRCS := $(wildcard $(SRC_DIR)/*.cpp) \
        imgui/imgui.cpp \
        imgui/imgui_draw.cpp \
        imgui/imgui_tables.cpp \
        imgui/imgui_widgets.cpp \
        imgui/imgui_demo.cpp \
        imgui/imgui_impl_sdl2.cpp \
        imgui/imgui_impl_opengl3.cpp
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

# Default target
all: $(BIN)

# Link objects into executable
$(BIN): $(OBJS)
	$(CXX) $(CXX_FLAGS) $(INCLUDES) -o $@ $^ $(FLAGS)

# Compile each .cpp into .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXX_FLAGS) $(INCLUDES) -c $< -o $@

# Clean build
clean:
	rm -rf $(OBJ_DIR) $(BIN)

# Run the program
run: all
	./$(BIN)

.PHONY: all clean run
