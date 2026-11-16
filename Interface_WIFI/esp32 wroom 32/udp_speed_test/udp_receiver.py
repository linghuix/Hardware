"""
PC-side receiver for udp_speed_test.ino

Usage:
    python udp_receiver.py <ESP32-IP> [rounds]
    python udp_receiver.py 192.168.137.110 3

Sends "start" to the ESP32 (UDP port 5005), then counts every packet that
actually arrives: received Mbps, lost packets (gaps in the sequence numbers)
and the ESP32's own count from its "END" packet.

If Windows Firewall asks whether Python may use the network, allow it on
private networks, otherwise no packets get through.
"""
import socket
import struct
import sys
import time

ESP_PORT = 5005


def drain(sock):
    """Throw away anything still queued, e.g. the extra copies of the previous
    round's END packet (the ESP32 sends END 5 times in case some get lost).
    Without this, round 2 read a stale END immediately and reported
    'no data packets received'."""
    sock.setblocking(False)
    try:
        while True:
            sock.recvfrom(2048)
    except (BlockingIOError, OSError):
        pass
    sock.settimeout(2.0)


def run_round(sock, esp_ip):
    drain(sock)
    sock.sendto(b"start", (esp_ip, ESP_PORT))
    print(f"sent 'start' to {esp_ip}:{ESP_PORT}, receiving...")

    received = 0          # data packets received
    bytes_rx = 0
    first_seq = None
    max_seq = -1
    t_first = t_last = None
    esp_sent = esp_ms = None
    sec_start = None
    sec_bytes = 0

    while True:
        try:
            data, _ = sock.recvfrom(2048)
        except socket.timeout:
            print("timeout: no data for 2 s")
            break
        now = time.perf_counter()

        if data.startswith(b"END"):
            if received == 0:
                continue   # stale END from an earlier round that slipped past drain()
            parts = data.decode(errors="ignore").split()
            if len(parts) >= 3:
                esp_sent, esp_ms = int(parts[1]), int(parts[2])
            break
        if len(data) < 4:
            continue

        seq = struct.unpack_from("<I", data)[0]
        if first_seq is None:
            first_seq = seq
            t_first = sec_start = now
        max_seq = max(max_seq, seq)
        received += 1
        bytes_rx += len(data)
        t_last = now

        # live per-second rate
        sec_bytes += len(data)
        if now - sec_start >= 1.0:
            print(f"  {sec_bytes * 8 / (now - sec_start) / 1e6:6.2f} Mbps")
            sec_start, sec_bytes = now, 0

    if received == 0:
        print("no data packets received (firewall? wrong IP? ESP32 not on hotspot?)")
        return

    duration = (t_last - t_first) if t_last > t_first else 1e-9
    expected = (esp_sent if esp_sent is not None else max_seq - first_seq + 1)
    lost = max(expected - received, 0)
    print("--- result (PC side) ---")
    print(f"received:   {received} packets, {bytes_rx} bytes in {duration:.2f} s")
    print(f"throughput: {bytes_rx * 8 / duration / 1e6:.2f} Mbps "
          f"({bytes_rx / 1024 / duration:.0f} KB/s)")
    if esp_sent is not None:
        print(f"ESP32 sent: {esp_sent} packets in {esp_ms} ms")
    else:
        print("ESP32 END packet not received; loss estimated from sequence numbers")
    print(f"lost:       {lost} packets ({100.0 * lost / max(expected, 1):.1f} %)")
    print("------------------------")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    esp_ip = sys.argv[1]
    rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 1

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Large receive buffer so the PC itself doesn't drop packets
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 8 * 1024 * 1024)
    sock.bind(("", 0))
    sock.settimeout(2.0)

    for i in range(rounds):
        if rounds > 1:
            print(f"\n=== round {i + 1}/{rounds} ===")
        run_round(sock, esp_ip)
        time.sleep(0.5)   # let the ESP32 finish sending its END copies (~100 ms)


if __name__ == "__main__":
    main()
