import socket
from datetime import datetime
import threading

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

import select  # for non-blocking socket operations on Windows

connectedClients = []
clients_lock = threading.Lock()

# Set the host and port for the server
host = '0.0.0.0'  # Use '0.0.0.0' for all available interfaces
port = 12345  # Choose a port number

# Create a socket object 
# socket.AF_INET specifies the address family (IPv4),
# socket.SOCK_STREAM specifies the socket type (TCP). 
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Set the receive buffer size (optional)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 8192000)  # 8192000 bytes (8000 KB)

#  enable the TCP_NODELAY option for the socket.
server_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

# Bind the socket to a specific address and port
server_socket.bind((host, port))

# Listen for incoming connections
server_socket.listen(5)

# Use Non-blocking Sockets
# server_socket.setblocking(False)

print(f"Server listening on {host}:{port}")

# Dictionaries to store data for each client
client_data = {}


def is_socket_connected(sock):
    try:
        # Check if the socket is still connected
        sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
        return True
    except socket.error as e:
        # An exception is raised if the socket is not connected
        return False

""" receive data stream split with @ """
def receive_until_at_symbol(client_socket):

    data = b''  # Initialize an empty byte string to store received data

    while True:
        chunk = client_socket.recv(1)  # Adjust the buffer size as needed
        data += chunk

        if b'@' in chunk:
            break  # Break the loop when "@" is found in the received data

    return data

""" Function to handle a single client """
def handle_client(client_socket, client_address):

    global connectedClients
    with clients_lock:
        connectedClients.append(client_socket)  # add new client
        print(f"new connection from {client_address}")

    try:
        num_bytes = 0
        floatData = []
        while is_socket_connected(client_socket):
            # Receive data from the client
            # data = client_socket.recv(12)
            data = receive_until_at_symbol(client_socket)
            # print('data - ', data)

            # Get the current timestamp with milliseconds
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-1]

            # Calculate the number of received bytes
            num_bytes = num_bytes+len(data)

            data_utf = data.decode('utf-8', errors='ignore')

            # Determine client identifier based on their IP address
            # client_identifier = f"{client_address[0]}_{client_address[1]}"
            client_identifier = f"client_{client_address[0].replace('.', '_')}_{client_address[1]}"

            # Store the received data in the dictionary
            if client_identifier not in client_data:
                client_data[client_identifier] = []

            # Convert the received data to a float
            try:
                value_str = data_utf.split('@')
                # Remove empty strings from the list
                cleaned_value_str = [value for value in value_str if value]

                float_data_list = [float(a) for a in cleaned_value_str]

                # merge two lists into one
                floatData = floatData + float_data_list

                client_data[client_identifier] = client_data[client_identifier] + float_data_list

            except ValueError:
                print(f"Invalid float value received: {data.decode()}")
                # continue

            # Print the received data with timestamp, byte count, and client identifier
            # print(f"[{timestamp}] Received {num_bytes} bytes from {client_identifier}: {data_utf}")
                
    except Exception as e:
        print(f"Exception in handle_client: {e}")
    finally:
        # Close the connection
        client_socket.close()
        print(f"Connection from {client_address} closed")

"""function to broadcast command to all connected clients"""
def broadcast_command(command):
    global connectedClients
    with clients_lock:
        for client in connectedClients:
            try:
                client.sendall((command + "\n").encode('ascii'))
            except:
                print("Error sending command to clients")

# Function to accept and handle multiple clients
def accept_clients():

    while True:
        try:
            # Wait for a connection
            client_socket, client_address = server_socket.accept()
            # print(f"Connection from {client_address}")

            # Create a new thread to handle the client
            client_thread = threading.Thread(target=handle_client, args=(client_socket, client_address))
            client_thread.start()

        except BlockingIOError:
            # Handle the non-blocking error
            readable, _, _ = select.select([server_socket], [], [], 1.0)  # Adjust timeout as needed

            #if not readable:
            #    continue  # Timeout, no incoming connections

# Create a thread to accept and handle clients
accept_clients_thread = threading.Thread(target=accept_clients)
accept_clients_thread.start()


""" Create a figure and axis for the plot """
MAX_EXPECTED_PLOTS = value = int(input("input MAX_EXPECTED_PLOTS number"))
fig, axs = plt.subplots(nrows=MAX_EXPECTED_PLOTS, sharex=True)

command = "SEND_DATA";
broadcast_command(command)
# Create a line object for the plot
# line, = axs[0].plot( [])
lines = {}
colors = ['b', 'g', 'r', 'c', 'm', 'y', 'k']


# Animation update function
def update_plot(frame):

    maxPointsToDraw = 200

    # Limit the number of data points to keep the plot responsive
    try:
        for client_name in list(client_data.keys()):

            client_index = list(client_data.keys()).index(client_name)

            if client_index not in list(lines.keys()):
                lines[client_index], = axs[client_index].plot([],[], label=f'Client {client_name}', color=colors[client_index % len(colors)])
                
                # Set plot labels and title
                axs[client_index].set_xlabel('Time')
                axs[client_index].set_ylabel('Value')
                # axs[client_index].set_title('Real-time Serial Data Plot')

                print('lines', lines)
            else:
                y_data = client_data[client_name]

                y_data_trunc = y_data[-maxPointsToDraw:]
                x_data_trunc = list( range(1, min(len(y_data) + 1, maxPointsToDraw+1) ))     # create 1:2000 list
                lines[client_index].set_data(x_data_trunc, y_data_trunc)

                # Automatically adjust the view limits
                axs[client_index].relim()
                axs[client_index].autoscale_view()
                # axs[client_index].set_ylim(0, 4096)
                # axs[client_index].set_xlim(0, maxPointsToDraw)
                axs[client_index].legend()
    except :
        pass


# Create an animation object
ani = FuncAnimation(fig, update_plot, interval=10, save_count=10)

from scipy.io import savemat
from datetime import datetime

try:
    # Show the plot
    plt.show()
except KeyboardInterrupt:
    # Handle Ctrl+C to gracefully exit the program

    print("Program terminated by user.")
finally:
    # Close the serial port and wait for the thread to finish
    # accept_clients_thread.join()

    # Generate a timestamp for the filename
    timestamp = datetime.now().strftime('%m%d_%H%M%S')

    # Construct the filename with the timestamp
    filename = f'./data_{timestamp}.mat'
    print(client_data)
    savemat(filename, client_data)
    print('#### save data ####')