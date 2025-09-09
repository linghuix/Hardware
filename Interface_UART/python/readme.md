# Real-Time Serial Data Plotter (Python)

A Python script to read numerical data from a serial device in real-time, plot it, and save it to a MATLAB `.mat` file.





##  Requirements

- Python 3.7+
- Libraries: `pyserial`, `matplotlib`, `numpy`, `scipy`  
  ```bash
  pip install pyserial matplotlib numpy scipy



##  serialGUI.m



**features** 

- Connects to a specified COM port and baud rate.

- Real-time plotting of multiple data lines.
- Saves collected data automatically on exit with a timestamp.

**Usage**

1. Set your serial port and baud rate:

   ```
   serial_port = "COM13"
   baud_rate = 115200
   ```

2. Set the number of data lines (`number_lines`).

3. Run the script:

   ```
   python serial_plot.py
   ```

4. Data is saved automatically on exit.

5. Data Format

​		Each line sent to computer should be space-separated numbers, e.g.:
```
12.3 45.6 78.9 23.4
```
