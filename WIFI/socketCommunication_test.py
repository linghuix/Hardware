import socket
from datetime import datetime

# Set the host and port for the server
host = '0.0.0.0'  # Use '0.0.0.0' for all available interfaces
port = 12345  # Choose a port number

# Create a socket object
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Bind the socket to a specific address and port
server_socket.bind((host, port))

# Listen for incoming connections
server_socket.listen(5)

print(f"Server listening on {host}:{port}")

# Wait for a connection
client_socket, client_address = server_socket.accept()
print(f"Connection from {client_address}")

# Dictionaries to store data for each client
client_data = {}
num_bytes = 0

while True:
    # Receive and print data from the client
    data = client_socket.recv(1024)

    # Calculate the number of received bytes
    num_bytes = num_bytes + len(data)

    # Get the current timestamp
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

    # print(f"Received data: {data.decode()}")
    # Print the received data with timestamp
    print(f"[{timestamp}] Received {num_bytes} bytes: {data.decode()}")

# Close the connection
client_socket.close()
