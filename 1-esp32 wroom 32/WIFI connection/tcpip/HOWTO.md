# ESP-IDF iperf with a bigger TCP send buffer

Copy of the ESP-IDF v5.5.1 example `examples/wifi/iperf` (original text in `README.md`).
Goal: compare against the Arduino TCP test (`../tcpip_speed_test`), which is limited by the
Arduino core's fixed 5 744 B TCP send buffer.

## What is different from Arduino

Set in `sdkconfig.defaults.esp32` (applied automatically when the target is esp32):

| Setting                            | Arduino core 2.0.11 | This project |
| ---------------------------------- | ------------------- | ------------ |
| `CONFIG_LWIP_TCP_SND_BUF_DEFAULT`  | 5744                | **65534**    |
| `CONFIG_LWIP_TCP_WND_DEFAULT`      | 5744                | **65534**    |
| WiFi TX/RX buffers, AMPDU          | defaults            | raised       |
| CPU / flash                        | 240 MHz / DIO       | 240 MHz / QIO 40 MHz |

To change it: `.\idf.ps1 menuconfig` → Component config → LWIP → TCP → *Default send buffer size*.

## Build and flash

Open PowerShell in this folder:

```powershell
.\idf.ps1 build
.\idf.ps1 -p COM5 flash monitor      # replace COM5 with your port (Device Manager)
```

`idf.ps1` loads ESP-IDF with its own Python and builds into `C:\Users\<you>\esp\build\tcpip_iperf`
(outside OneDrive). Leave the monitor with `Ctrl+]`.
If PowerShell refuses to run scripts: `powershell -ExecutionPolicy Bypass -File .\idf.ps1 build`.

## Run the test (nothing to install on the PC)

In the serial monitor (`iperf>` prompt), connect to WiFi. The password is typed here, not stored in code:

```
sta_connect <ssid> <password>
```

Note the ESP32's IP address from the log (`sta ip: ...`).

**ESP32 → PC (TCP TX, compare with the Arduino test)**

1. PC, start the receiver first:
   ```
   python -c "import socket,time;c,a=socket.create_server(('',5001)).accept();t=time.time();n=sum(iter(lambda:len(c.recv(65536)),0));d=time.time()-t;print(f'{n/1e6:.0f} MB in {d:.1f} s = {n*8/d/1e6:.1f} Mbit/s')"
   ```
2. ESP32 monitor:
   ```
   iperf -c <PC-IP> -i 1 -t 10
   ```

**PC → ESP32 (TCP RX)**

1. ESP32 monitor: `iperf -s -i 1`
2. PC:
   ```
   python -c "import socket,time;s=socket.create_connection(('ESP32-IP',5001));b=bytes(65536);e=time.time()+10;[s.sendall(b) for _ in iter(lambda:time.time()<e,False)];s.close()"
   ```
   The ESP32 prints the bandwidth per second.

Allow Python through Windows Firewall (private networks) when asked.
Use the same network as the Arduino tests (PC hotspot) so the results are comparable.
