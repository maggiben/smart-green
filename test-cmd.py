import time
import serial
import argparse

def send_ping_and_get_response(serial_port):
    try:
        # Command line parse
        parser = argparse.ArgumentParser(description='Serial Command Interface')
        parser.add_argument('-m', '--message', required=False, help='Message to send to the serial port')
        args = parser.parse_args()

        # Open the serial port
        ser = serial.Serial(serial_port, 115200, timeout=1)
        time.sleep(2)  # Wait for the serial connection to initialize

        # Send "ping" followed by a newline
        command = args.message or 'ping'
        print(f"command: {args.message}")
        ser.write(f"{command}\n".encode())

        # Wait for a response
        response = ser.readline().decode(errors='ignore').strip()

        # Print the response
        print(f"Received: {response}")

        # Close the serial port
        ser.close()

    except serial.SerialException as e:
        print(f"Error opening or using the serial port: {e}")

if __name__ == "__main__":
    # Specify the serial port
    serial_port = '/dev/ttyUSB0'
    send_ping_and_get_response(serial_port)

