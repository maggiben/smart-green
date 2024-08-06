#!/bin/bash
pio run --target upload --target nobuild --upload-port $(python3 find-serial-port.py)
