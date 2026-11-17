import socket

UDP_IP = "0.0.0.0"
UDP_PORT = 12348

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

total_bytes = 0
print("开始接收 UDP 数据...")

try:
    while True:
        data, addr = sock.recvfrom(2048)
        total_bytes += len(data)
        print(f"\r已接收: {total_bytes} 字节", end="")
except KeyboardInterrupt:
    print("\n接收结束")
    print(f"总共接收 {total_bytes} 字节")
