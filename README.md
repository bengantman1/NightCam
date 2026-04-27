# ECE 4180 - Final Project

Aidan Abrams, Benjamin Gantman

## <ins>Overview</ins><br>

The project is a self-contained, battery-powered smart trail and security camera built on the ESP32-S3 Sense microcontroller. The system is designed to autonomously monitor an outdoor area, activate only when motion is detected, adapt its behavior to ambient lighting conditions, and mechanically track a target using a motorized pan/tilt mount. Recorded footage is saved locally to an SD card and can be retrieved wirelessly via a built-in Wi-Fi HTTP file server. 

## <ins>Components</ins><br>

**ESP32-S3 sense** - The ESP32-S3 Sense is the MCU for the system. It runs FreeRTOS and manages all concurrent tasks including motion detection, image capture, image processing, servo control, environmental sensing, SD card writing, and Wi-Fi serving. Its integrated camera interface directly connects to the OV3660 sensor, and its dual-core Xtensa architecture allows camera and processing tasks to run in parallel. <br>

**OV3660** - This package also included a camera expansion card with an OV3660 camera. This camera was modified by removing the IR filter giving it the ability to “see” in the dark. Uses DMA to save the images quickly and reduce computational load.<br>

**PIR Motion Sensor** - A passive infrared (PIR) sensor monitors the environment for movement. It is connected to a GPIO interrupt line on the ESP32 so that when motion is detected, it triggers the camera and tracking tasks. This limits the use of the camera and tracking tasks which are very computationally expensive.<br>

**Photoresistor (LDR)** - A light-dependent resistor is read through the ESP32’s ADC as part of a voltage divider circuit. The measured voltage reflects ambient light level and is used to decide whether the IR illumination array needs to be activated.<br>

**IR Array** - Taken from an old trail cam, it is driven from a NPN BJT (PN2222) using a 9V battery since it requires more current than the S3 can supply. It is used to illuminate the FOV of the camera when the light is under a certain threshold.<br>

**Servo Motors** - Two servo motors control horizontal (pan) and vertical (tilt) movement of the camera mount. They receive position commands from the tracking task via the ESP32’s LEDC PWM peripheral. Position commands are derived from a vision processing task that computes the centroid or blob center of detected motion within each captured frame and calculates the angular offset required to center the target.<br>

**MicroSD Card** - Allows for storage of the images. It is accessed over SPI using the ESP32’s FATFS layer. Is written to after the detected motion has finished recording. Can be accessed through the Wi-Fi web server to download images or videos.<br>

**Push Button** - Interrupt to enable Wi-Fi and begin the access point for accessing the clips.<br>

**NPN BJT (PN2222)** - Used to turn on the IR array by connecting a GPIO pin to the base and setting it high. <br>

## <ins>Problems</ins><br>

Had issues with the ADC read on the LDR which was due to our voltage divider having too high an impedance for the ADC circuitry. Finding lower value resistors fixed the issue. <br>

The tracker has difficulty in noisy environments (varying backgrounds) or when one area is significantly brighter than its surroundings (ceiling lights), causing it to lock onto the brightest region rather than the moving subject. However, this same behavior benefits night vision tracking since the IR illuminates the center of the frame, making the subject the brightest object.<br>

Some challenges were encountered in implementing sleep mode on the ESP32-S3. The designated wake-up pins did not successfully wake the system and when the MCU was disconnected from power while in light or deep sleep, there were issues detecting the board on its COM port, flashing new software, or interacting with any of the peripherals, even after power cycling and resetting manually.<br>

## <ins>Comparisons</ins> <br>

This project sits between commercial trail cameras and networked security cameras. Like trail cameras, it uses PIR-triggered capture and local SD storage, but adds active pan/tilt tracking and wireless access that fixed trail cameras lack. Like some security cameras, it offers motion tracking and wireless footage retrieval, but unlike those systems it is fully battery-powered, requires no existing Wi-Fi network, and processes tracking locally. The combination of these features stands out among existing solutions with enhanced wildlife tracking and greater user convenience.<br>

## <ins>Improvements</ins> <br>

Investigating sleep/wake-up issues to improve overall power consumptionwould be a priority. Tweaking image quality and resolution to find the best balance without sacrificing tracking performance. The tracking algorithm itself could be improved, as it currently favors areas of high luminosity rather than strictly detecting movement. A faster JPEG decoder library, which we could not get working in time, would reduce decoding overhead and enable a higher tracking frame rate. A more significant design pivot would be offloading tracking to an external computer via video streaming, using ML-based object detection, which would remove the on-device processing bottleneck, allow higher resolution capture, and reduce sensitivity to environmental noise. <br>

## <ins>Circuit Diagram</ins> <br>

![Circuit Diagram](https://github.gatech.edu/user-attachments/assets/323d38a6-af50-4f7d-9e3c-a495e0a0e3a7)


