import socket
from datetime import datetime
import threading
import select
import pyqtgraph as pg
from pyqtgraph.Qt import mkQApp, QtCore
from scipy.io import savemat

import time

# Set the host and port for the server
host = '0.0.0.0'
port = 12345

# Create a socket object
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 81920000)
# server_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
server_socket.bind((host, port))
server_socket.listen(5)

print(f"Server listening on {host}:{port}")

client_data = {}

def is_socket_connected(sock):
    try:
        sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
        return True
    except socket.error as e:
        return False
    
def receive_until_at_symbol(client_socket):
    data = b''

    while True:
        chunk = client_socket.recv(1)
        data += chunk

        if b'@' in chunk:
            break

    return data

def handle_client(client_socket, client_address):
    try:
        num_bytes = 0
        floatData = []
        start_timestamp_estimated = 0;
        start_timestamp = time.time()

        while is_socket_connected(client_socket):
            data = receive_until_at_symbol(client_socket)
            timestamp = time.time()
            time_difference = int((timestamp - start_timestamp) * 1000);
            start_timestamp_estimated = start_timestamp_estimated + 10;
            num_bytes = num_bytes + len(data)
            data_utf = data.decode('utf-8', errors='ignore')
            client_identifier = f"client_{client_address[0].replace('.', '_')}"

            if client_identifier not in client_data:
                client_data[client_identifier] = []

            try:
                value_str = data_utf.split('@')
                cleaned_value_str = [value for value in value_str if value]
                float_data_list = [float(a) for a in cleaned_value_str]
                floatData = floatData + float_data_list
                client_data[client_identifier] = client_data[client_identifier] + float_data_list

                print(f"[{time_difference}] [{start_timestamp_estimated}]: {data_utf}")
                # Print the received data with timestamp, byte count, and client identifier
                # print(f"[{timestamp}] Received {num_bytes} bytes from {client_identifier}: {data_utf}")
            except ValueError:
                print(f"Invalid float value received: {data.decode()}")

    except Exception as e:
        print(f"Exception in handle_client: {e}")
    finally:
        client_socket.close()
        print(f"Connection from {client_address} closed")

def accept_clients():
    while True:
        try:
            client_socket, client_address = server_socket.accept()
            print(f"Connection from {client_address}")
            client_thread = threading.Thread(target=handle_client, args=(client_socket, client_address))
            client_thread.start()
        except BlockingIOError:
            readable, _, _ = select.select([server_socket], [], [], 1.0)

# Create a thread to accept and handle clients
accept_clients_thread = threading.Thread(target=accept_clients)
accept_clients_thread.start()

# Create a Qt Application
app = mkQApp()

# Create a PlotWidget for each client
plot_widgets = []

# Set the number of expected plots
MAX_EXPECTED_PLOTS = int(input("Input MAX_EXPECTED_PLOTS number"))

# Create a plot for each client
for i in range(MAX_EXPECTED_PLOTS):
    plot_widget = pg.plot(title=f'Client {i}', labels={'bottom': ('Time', 's'), 'left': ('Value', 'V')})
    plot_widget.setYRange(0, 4096)
    plot_widget.setXRange(0, 200)  # Adjust the range as needed
    plot_widgets.append(plot_widget)

# Create a dictionary to store PlotDataItems for each client
plot_data_items = {i: plot_widgets[i].plot(pen=pg.mkPen(color=pg.intColor(i), width=2))
                   for i in range(MAX_EXPECTED_PLOTS)}

# Update function for PyQtGraph plot
def update_plot():
    max_points_to_draw = 200

    for client_name, plot_data_item in zip(list(client_data.keys()), plot_data_items.values()):
        y_data = client_data[client_name]
        y_data_trunc = y_data[-max_points_to_draw:]
        x_data_trunc = list(range(1, min(len(y_data) + 1, max_points_to_draw + 1)))

        # Update data for each client
        plot_data_item.setData(x_data_trunc, y_data_trunc)

# Create a timer to update the plot
timer = QtCore.QTimer()
timer.timeout.connect(update_plot)
timer.start(10)  # Adjust the interval as needed

# Start the Qt event loop
app.exec_()

# Close the server socket and wait for the thread to finish
server_socket.close()
accept_clients_thread.join()

# Save data to a .mat file
timestamp = datetime.now().strftime('%m%d_%H%M%S')
filename = f'./data_{timestamp}.mat'
print(client_data)
savemat(filename, client_data)
print('#### save data ####')