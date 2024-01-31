""" 
author: linghui
python version: 3.7.4
lib:    pip install pyserial
        pip install matplotlib
        pip install numpy
        pip install scipy
recevied data format： value1, value2, value3, ...
"""

import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import threading

import numpy as np
from scipy.io import savemat


# Global variables to store data for plotting
number_lines = 6  # actual number of lines to draw from serial

global number, last_number,x_data,y_data;
x_data = []
y_data = [[] for i in range(number_lines)]
number = 0;
last_number = 0;



# Specify the serial port and baud rate
serial_port = "COM14"  # Replace with your serial port
baud_rate = 115200  # Set the baud rate of your UART communication

# Create a serial object
ser = serial.Serial(serial_port, baud_rate)


# Callback function to handle incoming data
def handle_data(data):
    global number, last_number,x_data,y_data

    data_str = data.decode('utf-8').strip()
    try:
        value_str = data_str.split(' ')
        value = [float(a) for a in value_str]
        # value = float(data_str)  # Assuming data is a float, modify accordingly
        # print(y_data)
        number = len(value); 
        if number is not last_number:
            x_data = []
            y_data = [[],[],[],[],[]]
        last_number = number;
        

        x_data.append(len(x_data))
        for order in range(number):
            y_data[order].append(value[order])

    except ValueError as e:
        print(f"Error parsing data: {e}")

# Function to read data in a separate thread
def read_serial_data(ser, callback):
    try:
        while True:
            # Read data from the serial port
            data = ser.readline()
            if data:
                # Call the callback function with the received data
                callback(data)
    except serial.SerialException as e:
        print(f"Serial communication error: {e}")


# Create a thread to read data
thread = threading.Thread(target=read_serial_data, args=(ser, handle_data))
thread.start()

# Create a figure and axis for the plot
fig, ax = plt.subplots()

# Create lines object for the plot
lines = [ax.plot([], [], label=f'Line {i}')[0] for i in range(number_lines)]

# Set plot labels and title
ax.set_xlabel('Time')
ax.set_ylabel('Value')
ax.set_title('Real-time Serial Data Plot')
ax.legend()


# Animation update function
def update_plot(frame):
    global number
    # Limit the number of data points to keep the plot responsive
    max_points = 1000
    x_data_trunc = x_data[-max_points:]

    # Update data for each line
    for order, line_obj in enumerate(lines[:number]):
        y_data_trunc = y_data[order][-max_points:]  # Truncate the data to the last 'max_points' values
        line_obj.set_data(x_data_trunc, y_data_trunc)

    # Automatically adjust the view limits
    ax.relim()
    ax.autoscale_view()
    ax.set_ylim(-200, 200)



# Create an animation object
ani = FuncAnimation(fig, update_plot, interval=100, save_count=5000)

try:
    # Show the plot
    plt.show()

except KeyboardInterrupt:
    # Handle Ctrl+C to gracefully exit the program

    print("Program terminated by user.")

finally:
    # Close the serial port and wait for the thread to finish
    ser.close()
    thread.join()
    
    # Save data to a MATLAB file when the program is terminated
    
    # Generate a timestamp for the filename
    timestamp = datetime.now().strftime('%Y%m%d%H%M%S')

    # Construct the filename with the timestamp
    filename = f'./data_{timestamp}.mat'
    data_dict = {'time_data': np.array(x_data), 'value_data': np.array(y_data)}
    
    savemat(filename, data_dict)
    
    print('#### save data ####')
