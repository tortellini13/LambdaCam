# LambdaCam

- [About Us](#about-us)
- [How to Build](#how-to-build)
- [Contributing to the Project](#contributing-to-the-project)
## About Us
### The Team
@tortellini13 and @n80n888 started this project as a senior capstone project while attending the University of Hartford. Coming into this project, neither of us had a lot of programming experience so a lot of what we've learned was though this project.
### The Project
An acoustical camera is a device that receives sound from an array of microphones and estimates the direction in which the sound is arriving. The acoustical camera then overlays a heatmap of the incoming sound data over a regular camera feed.

We wanted to create a low cost acoustic camera to give more people access to the technology. A commercial acoustic camera could cost $10,000+, which is generally way out of budget for most acoustical consultants or individuals.
# How to Build
More information about how to build the project is to come...
## Hardware
...
## Software
...

# Contributing to the Project
More information about contributing to the project is to come...
### How to Contribute
...
### Future Plans
- [ ] Hardware
	- [ ] Overhaul microphone array
		- [ ] Develop inexpensive microcontroller audio interface (XMOS)
		- [ ] Design new mic array with TDM MEMS microphones
		- [ ] Design arrays with nonlinear spacing
	- [ ] Carrier board
		- [ ] Design custom CM5 carrier board
		- [ ] Power solution for custom carrier board
	- [ ] Enclosures
		- [ ] Standalone 3D printed enclosure with easily swappable mic array
		- [ ] USB enclosure for mic arrays
- [ ] Software
	- [x] Restructure project
	- [ ] More OS support
		- [ ] Windows
		- [ ] IOS
		- [ ] Android
	- [ ] Redo Video
		- [ ] Redesign GUI to be more user friendly
		- [ ] Make the GUI more modular so it is easy to add new features
		- [ ] Add some user customization (colors)
		- [ ] Make GUI dynamically sized
	- [ ] Create a menu to select sound device during runtime
	- [ ] Explore other methods of beamforming
	- [ ] Debug utilities
		- [ ] Add debug options during runtime
		- [ ] Remove all debugging options that require compile time changes
		- [ ] Add more debug information options
	- [ ] GPU acceleration with compute shaders for beamforming math
	- [ ] Companion app
	- [x] Use JSON for configs
	- [ ] Make x button stop the program correctly
	- [ ] Add the ability to switch to recording playback during runtime
	- [ ] Make selection for recording playback
	- [ ] Recording and playback
		- [ ] Synchronous video and audio recording
		- [ ] Saving video and audio files
		- [ ] File path selection
		- [ ] Video and audio playback
		- [ ] Listen to beamformed sound at a location
- [ ] Other
	- [ ] Add pictures and videos to README
	- [ ] Finish writing README
	- [ ] Make actual release

### GUI Planning
Checked = Implemented feature
Highlighted = Awaiting feature, control in place

- [ ] Buttons
	- [ ] Start/stop recording
	- [ ] Screenshot
- [ ] Sliders
	- [ ] Frequency selection (with FFT display)
	- [ ] Min
	- [ ] Max
	- [ ] Threshold
	- [ ] Midpoint?
- [ ] Main Menu
	- [ ] File
		- [ ] ==New Recording==
		- [ ] ==Save Recording==
		- [ ] ==Open Recording==
	- [ ] Settings
		- [ ] ==Select Audio Device==
	- [ ] Overlay
		- [ ] ==Toggle Heatmap==
		- [ ] ==Toggle Heatmap Legend==
		- [ ] ==Toggle Max Cursor==
		- [ ] ==Light/Dark Mode==
- [ ] Display
	- [ ] Heatmap Legend
	- [ ] Debug menu (popout?)
		- [ ] FPS counters for all components
		- [ ] Display information
		- [ ] Audio information
	- [ ] Maximum heatmap value
	- [ ] Cursor to track maximum