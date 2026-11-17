
A record of Raspberry Pi WiFi speed tests: what was measured and how.

### TCP throughput, Pi → PC (2026-11-16)

**Hardware**

| Part     | Details                                    |
| -------- | ------------------------------------------ |
| Board    | Raspberry Pi, model: _fill in_             |
| Network  | (router , 2.4 GHz)                         |
| software | Built-in `python3` only, nothing installed |

**Method**

The Pi sends zero-filled 64 KB blocks over one TCP connection for 10 s and the PC counts the bytes it receives. Only Python's standard library is used, so nothing has to be installed on either side.

PC (receiver, start first):
```
python -c "import socket,time;c,a=socket.create_server(('',5001)).accept();t=time.time();n=sum(iter(lambda:len(c.recv(65536)),0));d=time.time()-t;print(f'{n/1e6:.0f} MB in {d:.1f} s = {n*8/d/1e6:.1f} Mbit/s')"
```

Raspberry Pi (sender, 10 s):
```
python3 -c "import socket,time;s=socket.create_connection(('PC-IP',5001));b=bytes(65536);e=time.time()+10;[s.sendall(b) for _ in iter(lambda:time.time()<e,False)];s.close()"
```

To test the other direction (PC → Pi), swap the two commands.

**TCP/IP Measurements**

| Run | Data received | Time   | Result            |
| --- | ------------- | ------ | ----------------- |
| 1   | 30 MB         | 10.5 s | **22.8 Mbit/s**   |
| 2   | 36 MB         | 11.3 s | **25.3 Mbit/s**   |
| 3   | 33 MB         | 10.7 s | **24.9 Mbit/s**   |
| Avg |               |        | **~24.3 Mbit/s** (~3.0 MB/s) |

Runs took slightly longer than 10 s because the last buffered data drains after the sender stops, so the numbers are a few percent on the low side.

**Link info** (`iw dev wlan0 link` on the Pi): 
```

pi@pi:~ $ iw dev wlan0 link
Connected to 8c:a6:df:96:eb:5f (on wlan0)
        SSID: TP-LINK_LHX
        freq: 2412
        RX: 1322301149 bytes (22476289 packets)
        TX: 3064185816 bytes (28697454 packets)
        signal: -11 dBm
        rx bitrate: 72.2 MBit/s
        tx bitrate: 72.2 MBit/s

        bss flags:      short-preamble short-slot-time
        dtim period:    1
        beacon int:     100

```

Look at three lines:

- `freq`: 24xx means 2.4 GHz and 5xxx means 5 GHz. 
- `tx bitrate`: about 72 Mbit/s or less means 24 Mbit/s is normal for this link.That’s the rate the Pi and router agreed on, not the real throughput. Real TCP speed is usually 40–60 % of it.
- `signal`: anything above about −60 dBm is fine. Weaker than −70 dBm means move the Pi closer.

---

### Disscussion


1. **The Pi reaches ~24 Mbit/s over TCP, about 4× the ESP32** (5.3–6.4 Mbps TCP on the PC hotspot, see `1-esp32 wroom 32/test_LOG.md`). The Pi's Linux TCP stack has no 5.7 KB send-buffer limit like the ESP32 Arduino core.

2. **~24 Mbit/s is typical for a 2.4 GHz single-stream link** (~65–72 Mbit/s link rate, TCP gets 40–60 % of it). On 5 GHz a Pi 4/5 usually reaches 100+ Mbit/s.

3. Possible improvements: switch the network to 5 GHz, and turn off WiFi power saving with `sudo iw wlan0 set power_save off` (until reboot).

