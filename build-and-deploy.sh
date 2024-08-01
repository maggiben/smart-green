#!/bin/bash

# Define variables
ENV_NAME="nodemcu-32s"
RASPBERRY_PI_USER="bmaggi"
RASPBERRY_PI_IP="0.0.0.0"
REMOTE_PATH="/smart-water/firmware/.pio/build/$ENV_NAME"
REMOTE_PORT="/dev/ttyUSB0"

# Compile the binary
~/.platformio/penv/bin/pio run -e $ENV_NAME

if [ $? -eq 0 ]; then
    # Copy the binary to the Raspberry Pi
    echo "Build succeded deploying"
    scp .pio/build/$ENV_NAME/firmware.bin .pio/build/$ENV_NAME/firmware.elf $RASPBERRY_PI_USER@$RASPBERRY_PI_IP:$REMOTE_PATH
    # Light version for prod # scp .pio/build/$ENV_NAME/firmware.bin $RASPBERRY_PI_USER@$RASPBERRY_PI_IP:$REMOTE_PATH
else
    echo "Build process failed"
fi
