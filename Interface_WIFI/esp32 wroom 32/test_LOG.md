
A record of every step taken, what was measured, and why each change was made.

### Step 9. tcpip_speed_test.ino (TCP) 

**Hardware**

| Part     | Details                                                                          |
| -------- | -------------------------------------------------------------------------------- |
| Board    | AZ-Delivery ESP32 dev board, ESP32-WROOM-32, **no PSRAM** (~320 KB internal RAM) |
| software | Arduino IDE                                                                      |

**TCP/IP Measurements**

`Interface_WIFI/tcpip_speed_test/tcpip_speed_test.ino`: ESP32 TCP server on port 8080 that sends as fast as possible in 5 seconds.

| Test               | Setup                                  | Result                                             |
| ------------------ | -------------------------------------- | -------------------------------------------------- |
| TCP, 4096-B writes | Home router `TP-LINK_LHX`, −57…−60 dBm | **23–60 KB/s** (0.19–0.49 Mbps)                    |
| Ping PC → ESP32    | Home router                            | **3–124 ms**, median ~9 ms, average ~25 ms, 0 loss |
| TCP, 4096-B writes | **PC hotspot** `GGGG`, −22…−29 dBm     | **641–779 KB/s** (5.3–6.4 Mbps)                    |
| Ping PC → ESP32    |                                        | 最短 = 1ms，最长 = 66ms，平均 = 8ms                        |

#### TCP throughput on this setup
```
TCP speed ≈ send buffer ÷ round-trip time = 5 744 B ÷ RTT
```
In  Arduino, the send buffer is fixed to 5.7 KB. 

| RTT | TCP speed |
|---|---|
| ~8 ms (PC hotspot) | ~700 KB/s ← matches measurement |
| 25–120 ms + lost packets (router) | tens of KB/s ← matches measurement |

*In offical documents, the tcp ip speed can reach up to 24 Mbps.* that is because the send buffer can be set to much large in espif IDE.

**So to increase speed you can use espif IDE.**

---


### Step 11. UDP Measurements

| UDP (`udp_speed_test.ino` + `udp_receiver.py`), 1400-B packets | PC hotspot                             | **17.6–19.2 Mbps** (2.15–2.34 MB/s), **0 % loss**  |
| -------------------------------------------------------------- | -------------------------------------- | -------------------------------------------------- |

`udp_speed_test`: the PC script sends `start`, the ESP32 blasts sequence-numbered packets for 5 s, the PC counts what actually arrives.
(Fixed one bug: leftover `END` packets made rounds 2–3 report no data.)



## What we learned


### Three conclusions

1. **The biggest problem about such low speed in TCPIP was the router network** (jittery latency and packet loss). Switching to the PC hotspot gave a **10–30×** TCP speed-up.

2. **The Arduino core's 5.7 KB TCP buffer costs about two-thirds of the chip's speed.** UDP is not limited by it and reached ~18 Mbps (Espressif's tuned TCP tests reach 20–30 Mbps).



### Files
| File                                                                                    | Purpose                                                       |
| --------------------------------------------------------------------------------------- | ------------------------------------------------------------- |
| `Interface_WIFI/NetworkTest/NetworkTest.ino`                                            | TCP throughput test                                           |
| `Interface_WIFI/udp_speed_test/udp_speed_test.ino`                                      | UDP throughput test (ESP32 side)                              |
| `Interface_WIFI/udp_speed_test/udp_receiver.py`                                         | UDP throughput test (PC side)                                 |
