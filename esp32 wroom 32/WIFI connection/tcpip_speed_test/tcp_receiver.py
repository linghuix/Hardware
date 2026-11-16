"""
PC-side receiver for tcpip_speed_test.ino  (same structure as udp_receiver.py)

Usage:
    python tcp_receiver.py <ESP32-IP> [rounds]
    python tcp_receiver.py 192.168.137.110 3

1. Connects to the ESP32 (TCP port 8080).
2. Measures the idle round-trip time (RTT): 20 x "ping" -> "pong".
3. Each round: sends "start", receives fixed-size blocks for ~5 s, checks the
   sequence numbers, prints the live rate, then the ESP32's own statistics
   from the end block.

TCP never loses data, so instead of packet loss this shows:
  - throughput            (limited to ~5 744 B / RTT by the Arduino core)
  - stalls                (a send() that took > 300 ms = lost packet + retransmit)
  - expected max speed    (5 744 B / measured RTT), to compare with the real one

If Windows Firewall asks whether Python may use the network, allow it on
private networks.
"""
import socket        # TCP networking
import statistics    # median / mean for the RTT measurements
import struct        # unpack the 4-byte little-endian sequence number
import sys           # command-line arguments
import time          # high-resolution timer (perf_counter) and sleep

ESP_PORT = 8080              # TCP port the ESP32 listens on (TCP_PORT in the .ino)
BLOCK_SIZE = 4096            # must match BLOCK_SIZE in tcpip_speed_test.ino
END_SEQ = 0xFFFFFFFF         # sequence number that marks the end block of a round
TCP_SND_BUF = 5744           # Arduino ESP32 core 2.x CONFIG_LWIP_TCP_SND_BUF_DEFAULT


def recv_exact(sock, n):
    """TCP is a byte stream: keep reading until exactly n bytes arrived."""
    buf = bytearray()                            # collects the bytes received so far
    while len(buf) < n:                          # until we have the full n bytes
        chunk = sock.recv(n - len(buf))          # ask only for what is still missing
        if not chunk:                            # empty result = the other side closed
            raise ConnectionError("ESP32 closed the connection")
        buf += chunk                             # append this piece
    return bytes(buf)                            # exactly n bytes


def measure_rtt(sock, count=20):
    """Round-trip time of a tiny message over the idle TCP connection."""
    times = []                                   # RTT of each ping, in ms
    for _ in range(count):                       # repeat count times
        t0 = time.perf_counter()                 # start the timer
        sock.sendall(b"ping\n")                  # ESP32 answers "pong\n" immediately
        if recv_exact(sock, 5) != b"pong\n":     # wait for the 5-byte reply
            raise ValueError("unexpected reply to ping")
        times.append((time.perf_counter() - t0) * 1000)  # elapsed time in ms
        time.sleep(0.05)                         # small gap so pings don't queue up
    print(f"RTT (idle, {count} pings): min {min(times):.1f} ms, "
          f"median {statistics.median(times):.1f} ms, "
          f"avg {statistics.mean(times):.1f} ms, max {max(times):.1f} ms")
    med = statistics.median(times)               # median is robust to a few spikes
    # TCP can have at most TCP_SND_BUF bytes in flight per round trip:
    # speed limit = TCP_SND_BUF / RTT
    print(f"  -> expected TCP ceiling ~ {TCP_SND_BUF} B / {med:.1f} ms "
          f"= {TCP_SND_BUF / (med / 1000) / 1024:.0f} KB/s")
    return times                                 # returned for possible later use


def run_round(sock):
    """One test round: ask the ESP32 to send for ~5 s and measure what arrives."""
    sock.sendall(b"start\n")                     # tell the ESP32 to start sending
    print("sent 'start', receiving...")

    blocks = 0               # data blocks received in this round
    bytes_rx = 0             # data bytes received in this round
    expected_seq = 0         # sequence number the next block should have
    seq_errors = 0           # blocks out of order / missing (should stay 0 with TCP)
    t_first = None           # time the first data block arrived
    sec_start = None         # start of the current 1-second window (live rate)
    sec_bytes = 0            # bytes received in the current 1-second window
    esp_stats = None         # [blocks, ms, stalls, max_ms] from the ESP32's end block

    while True:                                  # read blocks until the end block
        data = recv_exact(sock, BLOCK_SIZE)      # one complete block
        now = time.perf_counter()                # arrival time
        seq = struct.unpack_from("<I", data)[0]  # first 4 bytes = little-endian uint32

        if seq == END_SEQ:                       # end block: parse the ESP32 statistics
            # text after the 4-byte header, up to the first zero byte
            text = data[4:].split(b"\0", 1)[0].decode(errors="ignore").split()
            if len(text) >= 5 and text[0] == "END":   # "END blocks ms stalls max_ms"
                esp_stats = [int(x) for x in text[1:5]]
            t_last = now                         # end of the measured interval
            break                                # round finished

        if t_first is None:                      # first data block of the round
            t_first = sec_start = now            # start both timers here
        if seq != expected_seq:                  # gap or reordering
            seq_errors += 1                      # should never happen with TCP
        expected_seq = seq + 1                   # what the next block should be
        blocks += 1                              # count the block
        bytes_rx += len(data)                    # count its bytes

        sec_bytes += len(data)                   # bytes in the current 1-s window
        if now - sec_start >= 1.0:               # a second has passed: print live rate
            print(f"  {sec_bytes * 8 / (now - sec_start) / 1e6:6.2f} Mbps")
            sec_start, sec_bytes = now, 0        # start the next window

    if blocks == 0:                              # end block arrived without any data
        print("no data blocks received")
        return

    duration = max(t_last - t_first, 1e-9)       # seconds from first block to end block
    print("--- result (PC side) ---")
    print(f"received:   {blocks} blocks, {bytes_rx} bytes in {duration:.2f} s")
    print(f"throughput: {bytes_rx * 8 / duration / 1e6:.2f} Mbps "    # bits / s -> Mbps
          f"({bytes_rx / 1024 / duration:.0f} KB/s)")                  # bytes / s -> KB/s
    print(f"data check: {'OK, all blocks in order' if seq_errors == 0 else f'{seq_errors} sequence errors!'}")
    if esp_stats:                                # ESP32's own numbers, if the end block parsed
        sent, ms, stalls, max_ms = esp_stats
        print(f"ESP32 sent: {sent} blocks in {ms} ms")
        print(f"stalls:     {stalls} (send > 300 ms), slowest send {max_ms} ms")
    print("------------------------")


def main():
    if len(sys.argv) < 2:                        # no IP given: show usage and stop
        print(__doc__)
        sys.exit(1)
    esp_ip = sys.argv[1]                         # ESP32 address printed on its serial port
    rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 1   # number of rounds (default 1)

    sock = socket.create_connection((esp_ip, ESP_PORT), timeout=10)  # connect, 10 s timeout
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)       # send "ping"/"start" immediately
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)  # big buffer: PC is never the bottleneck
    print(f"connected to {esp_ip}:{ESP_PORT}")

    try:
        measure_rtt(sock)                        # step 1: idle round-trip time
        for i in range(rounds):                  # step 2: the throughput rounds
            if rounds > 1:
                print(f"\n=== round {i + 1}/{rounds} ===")
            run_round(sock)
            time.sleep(0.5)                      # short pause between rounds
    finally:
        sock.close()                             # always close; the ESP32 then waits for a new client


if __name__ == "__main__":                       # run main() only when started as a script
    main()
