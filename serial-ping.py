import time
import serial
import argparse
import glob
import sys

def find_serial_port():
    ports = glob.glob('/dev/ttyUSB*')
    for port in ports:
        try:
            ser = serial.Serial(port, 115200, timeout=1)
            ser.close()  # Close the port if it opens successfully
            return port
        except serial.SerialException:
            continue
    return None

def send_ping_and_get_response():
    try:
        # Command line parse
        parser = argparse.ArgumentParser(description='Serial Command Interface')
        parser.add_argument('-m', '--message', required=False, help='Message to send to the serial port')
        args = parser.parse_args()

        # Find the first available serial port
        serial_port = find_serial_port()
        if serial_port is None:
            print("No available serial port found.", file=sys.stderr)
            return

        # Open the serial port
        ser = serial.Serial(serial_port, 115200, timeout=1)
        time.sleep(2)  # Wait for the serial connection to initialize

        # Send "ping" followed by a newline
        command = args.message or 'ping'
        ser.write(f"{command}\n".encode())

        # Wait for a response
        response = ser.readline().decode(errors='ignore').strip()

        # Print the response
        if response: 
            print(response)

        # Close the serial port
        ser.close()

    except serial.SerialException as e:
        print(f"Error opening or using the serial port: {e}", file=sys.stderr)

if __name__ == "__main__":
    send_ping_and_get_response()