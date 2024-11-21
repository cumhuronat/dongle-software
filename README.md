# USB Dongle Firmware for OpenPendant

This repository contains the firmware for the USB dongle component of the OpenPendant system. The dongle serves as a bridge between a computer and the OpenPendant device, enabling seamless communication and control.

## Overview

The USB dongle firmware is built using the ESP-IDF framework and is designed to OSS OpenPendant dongle hardware. It handles USB communication with the host computer and wireless communication with the pendant device.

## Hardware

The hardware designs for this project, including PCB layouts and enclosure models, are open-source and available at:
https://github.com/cumhuronat/mr-1-openpendant/tree/main/dongle

You can find:
- PCB design files (.pcb)
- 3D printable enclosure files (.f3d)

## Features

- USB CDC (Communications Device Class) implementation for computer connectivity
- Wireless communication with the OpenPendant device
- Real-time data transmission and reception
- Configurable settings for different pendant modes
- Automatic device discovery and pairing
- Firmware update capability

## Prerequisites

To build and flash this firmware, you'll need:

- ESP-IDF development framework
- Python 3.x
- CMake
- A compatible USB-to-Serial adapter (for initial flashing)
- Visual Studio Code with ESP-IDF extension (recommended)

## Project Structure

```
dongle-software/
├── components/         # Custom and third-party components
├── main/              # Main application source code
├── managed_components/# ESP-IDF managed components
├── CMakeLists.txt     # Project CMake configuration
├── sdkconfig          # Project configuration
└── reboot_and_flash.py# Utility script for flashing
```

## Building and Flashing

1. Set up the ESP-IDF environment
2. Clone this repository
3. Configure the project (optional):
   ```
   idf.py menuconfig
   ```
4. Build the project:
   ```
   idf.py build
   ```
5. Flash the firmware:
   ```
   idf.py -p [PORT] flash
   ```
   Or use the provided `reboot_and_flash.py` script for automated flashing.


## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Support

For questions and support, please open an issue in the GitHub repository.
