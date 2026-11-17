
A record of every step taken, what was measured, and why each change was made.

### Test 1.0. tcpip_speed_test.ino (TCP) 

**Hardware**

| Part     | Details                                                                          |
| -------- | -------------------------------------------------------------------------------- |
| Board    | AZ-Delivery ESP32 dev board, ESP32-WROOM-32, **no PSRAM** (~320 KB internal RAM) |
| software | Arduino IDE                                                                      |
| Code     | tcpip_speed_test.ino and tcp_receiver.py                                         |

**TCP/IP Measurements**

`Interface_WIFI/tcpip_speed_test/tcpip_speed_test.ino`: ESP32 TCP server on port 8080 that sends as fast as possible in 5 seconds.

| Test               | Setup                                  | Result                                             |
| ------------------ | -------------------------------------- | -------------------------------------------------- |
| TCP, 4096-B writes | Home router `TP-LINK_LHX`, −57…−60 dBm | **23–60 KB/s** (0.19–0.49 Mbps)                    |
| Ping PC → ESP32    | Home router                            | **3–124 ms**, median ~9 ms, average ~25 ms, 0 loss |
| TCP, 4096-B writes | **PC hotspot** `GGGG`, −22…−29 dBm     | **641–779 KB/s** (5.3–6.4 Mbps)                    |
| Ping PC → ESP32    |                                        | 最短 = 1ms，最长 = 66ms，平均 = 8ms                        |

**TCP throughput on this setup**
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


### Test 1.1. UDP Measurements

**environment**

| Part     | Details                                                                          |
| -------- | -------------------------------------------------------------------------------- |
| Board    | AZ-Delivery ESP32 dev board, ESP32-WROOM-32, **no PSRAM** (~320 KB internal RAM) |
| software | Arduino IDE                                                                      |
| Code     | udp_speed_test.ino and udp_receiver.py                                           |

**Results** 

| UDP (`udp_speed_test.ino` + `udp_receiver.py`), 1400-B packets | PC hotspot                             | **17.6–19.2 Mbps** (2.15–2.34 MB/s), **0 % loss**  |
| -------------------------------------------------------------- | -------------------------------------- | -------------------------------------------------- |

`udp_speed_test`: the PC script sends `start`, the ESP32 blasts sequence-numbered packets for 5 s, the PC counts what actually arrives.
(Fixed one bug: leftover `END` packets made rounds 2–3 report no data.)



**What we learned**

1. **The biggest problem about such low speed in TCPIP was the router network** (jittery latency and packet loss). Switching to the PC hotspot gave a **10–30×** TCP speed-up.

2. **The Arduino core's 5.7 KB TCP buffer costs about two-thirds of the chip's speed.** UDP is not limited by it and reached ~18 Mbps (Espressif's tuned TCP tests reach 20–30 Mbps).


	定义在 `C:\Users\lhx\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.11\tools\sdk\esp32\dio_qspi\include\sdkconfig.h`
	```
	#define CONFIG_LWIP_TCP_SND_BUF_DEFAULT 5744
	```
 在 Arduino 里改不了

Arduino 核心里的 lwIP 网络栈是**预编译好的库**（`.a` 文件），这个值在编译库的时候就定死了。你在 sketch 里 `#define` 一个新值不会生效，用 `setsockopt(SO_SNDBUF)` 也改不了。想改只有两条路：

- 用 ESP-IDF 自己编译，在 `menuconfig` 里改；
- 用 Arduino 官方的 Lib Builder 重新编译核心库。


---

### Step 2. ESP-IDF iperf example with a 64 KB TCP send buffer (2026-11-17)

**New test environment**

| Part           | Details                                                                                                              |
| -------------- | -------------------------------------------------------------------------------------------------------------------- |
| Board          | Same AZ-Delivery ESP32-WROOM-32, no PSRAM                                                                            |
| Software       | **ESP-IDF v5.5.1** (instead of Arduino IDE), official example `examples/wifi/iperf`                                  |
| Project folder | `WIFI connection/tcpip` (build with `.\idf.ps1 build`, flash with `.\idf.ps1 -p COMx flash monitor`, see `HOWTO.md`) |
| PC side        | Python one-liner TCP receiver on port 5001 (nothing installed), same as the Raspberry Pi test                        |
| ESP32 command  | `sta_connect <ssid> <password>` then `iperf -c <PC-IP> -i 1 -t 10` (ESP32 sends, PC receives = TCP TX)               |

Key settings (from `sdkconfig.defaults.esp32`), compared with the Arduino core 2.0.11:

| Setting                           | Arduino IDE   | ESP-IDF iperf |
| --------------------------------- | ------------- | ------------- |
| `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` | 5744 B        | **65534 B**   |
| `CONFIG_LWIP_TCP_WND_DEFAULT`     | 5744 B        | **65534 B**   |
| WiFi TX/RX buffers, AMPDU         | default       | raised        |
| lwIP code in IRAM                 | no            | yes           |
| WiFi power save                   | default       | off (`WIFI_PS_NONE`) |
| CPU / flash                       | 240 MHz / DIO | 240 MHz / QIO 40 MHz |

Memory after build (`idf.py size`): IRAM 84 % used, DRAM 19 % static (146 KB left for the heap, where the TCP buffers are allocated at run time), image 906 KB.



**Results - TCP/IP Measurements (ESP32 → PC)**

| Network                                                   | Result                                                       |
| --------------------------------------------------------- | ------------------------------------------------------------ |
| Home router `TP-LINK_LHX`, channel 1, 40 MHz, RSSI −57 dBm | **1.27 Mbps** average; 0.4–1.9 Mbps per second, one full stall (0.00 Mbps at 8–9 s) |
| PC hotspot `GGGG`                                          | **33.4 Mbps** (42 MB in 10.0 s, ≈ 4.2 MB/s)                  |


**Comparison with the Arduino tests and Espressif's official results**

| Test                                      | Router `TP-LINK_LHX` | PC hotspot `GGGG`  |
| ----------------------------------------- | -------------------- | ------------------ |
| Arduino IDE, TCP (5.7 KB buffer)          | 0.19–0.49 Mbps       | 5.3–6.4 Mbps       |
| Arduino IDE, UDP                          | –                    | 17.6–19.2 Mbps     |
| **ESP-IDF iperf, TCP (64 KB buffer)**     | **1.27 Mbps**        | **33.4 Mbps**      |

Official ESP32 best results (ESP-IDF v5.5 docs, *Wi-Fi Driver → ESP32 Wi-Fi Throughput*, same iperf example and `sdkconfig.defaults.esp32`):

| Type   | Air in Espressif's lab | Shielded box |
| ------ | ---------------------- | ------------ |
| TCP TX | 20 Mbps                | 75 Mbps      |
| TCP RX | 20 Mbps                | 65 Mbps      |
| UDP TX | 30 Mbps                | 75 Mbps      |
| UDP RX | 30 Mbps                | 85 Mbps      |

**What we learned (ESP-IDF)**

1. **The 64 KB send buffer gives a 5–6× TCP speed-up** over the Arduino core on the same hotspot (33.4 vs 5.3–6.4 Mbps), and 3–6× on the router. This confirms that the Arduino core's fixed 5 744 B buffer was the TCP bottleneck.
2. **TCP with ESP-IDF is now faster than UDP with Arduino** (33.4 vs ~18 Mbps); the Arduino UDP test was limited by its own send loop, not the radio.
3. **33.4 Mbps beats Espressif's official over-the-air result (20 Mbps)** and reaches about 45 % of the shielded-box best (75 Mbps). The remaining gap is the RF environment (interference, other networks), not the configuration, because the settings are identical to Espressif's test.
4. **The network still matters more than the buffer**: with the same firmware, the home router gave 1.27 Mbps and the hotspot 33.4 Mbps (≈ 25×). The router uses a 40 MHz channel on crowded 2.4 GHz channel 1 and loses packets (full stalls). Setting it to 20 MHz on a free channel (1/6/11) or using the hotspot is needed for high speed.
5. For a real application that needs fast TCP, use ESP-IDF (or rebuild the Arduino core with the Lib Builder) and raise `LWIP_TCP_SND_BUF_DEFAULT` / `LWIP_TCP_WND_DEFAULT`, keeping an eye on free DRAM since the board has no PSRAM.
