import sys
import serial
import serial.tools.list_ports
import time
import math
from PyQt5.QtWidgets import (QApplication, QWidget, QLabel, QComboBox, QPushButton, QLineEdit,
                             QCheckBox, QVBoxLayout, QHBoxLayout, QGridLayout, QFileDialog, QMessageBox)
from PyQt5.QtCore import QTimer, Qt
import matplotlib.pyplot as plt
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
import numpy as np
import pandas as pd
from datetime import datetime

import json, os



class SerialGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Thunderbird-style Serial Port Data Processing GUI")
        self.resize(1200, 700)
        
        # 配置文件路径
        config_file = 'config.json'

        # 默认配置
        default_config = {
            "StopCommand": "p\n",
            "colNames": [
                "No", "I", "V", "P", "filtered_I", "filtered_V",
                "setpoint", "cable_force", "output", "time"
            ],
            "PLOT_MAX_POINTS": 500,
        }

        # 如果配置文件不存在，则新建
        if not os.path.exists(config_file):
            with open(config_file, 'w') as f:
                json.dump(default_config, f, indent=4)
            print(f"'{config_file}' not found. Created default config.")
        
        # 读取配置
        with open('config.json', 'r') as f:
            config = json.load(f)

        self.StopCommand = config['StopCommand'].encode()  # 转成 bytes
        self.colNames = config['colNames']
        self.MAX_POINTS = config['PLOT_MAX_POINTS']
        
        # ---- Default configuration ----
        # self.StopCommand = b'p\n'
        # self.colNames = ['No','I','V','P','filtered_I','filtered_V','setpoint','cable_force','output','time']
        # self.colNames = ['No','I','receive','time']
        self.NUMBER_ = len(self.colNames)
        
        # Data buffers
        self.dataTbl = pd.DataFrame(columns=self.colNames)
        self.buf = np.empty((0, self.NUMBER_))
        self.times_arr = []

        # Serial variables
        self.sp = None
        self.isRunning = False

        # Setup UI
        self.initUI()
        
        # Plot timer for live updates
        self.timer = QTimer()
        self.timer.timeout.connect(self.updatePlots)
        self.timer.start(10)  # update every 100ms
        
        self.sintimer = QTimer()
        self.sintimer.timeout.connect(self.sendSin)

    def initUI(self):
        layout = QHBoxLayout(self)
        
        # Left panel: Controls
        left_layout = QVBoxLayout()
        
        # Serial port config
        grid = QGridLayout()
        grid.addWidget(QLabel("Port:"), 0, 0)
        self.ddPort = QComboBox()
        self.refreshPorts()
        grid.addWidget(self.ddPort, 0, 1)
        self.btnRefresh = QPushButton("Refresh")
        self.btnRefresh.clicked.connect(self.refreshPorts)
        grid.addWidget(self.btnRefresh, 0, 2)
        
        grid.addWidget(QLabel("Baud:"), 1, 0)
        self.ddBaud = QComboBox()
        self.ddBaud.addItems(['115200','128000','230400','256000','460800','921600'])
        self.ddBaud.setCurrentText('115200')
        grid.addWidget(self.ddBaud, 1, 1)
        
        grid.addWidget(QLabel("DataBits:"), 2, 0)
        self.ddDataBits = QComboBox()
        self.ddDataBits.addItems(['7','8'])
        self.ddDataBits.setCurrentText('8')
        grid.addWidget(self.ddDataBits, 2, 1)
        
        grid.addWidget(QLabel("StopBits:"), 3, 0)
        self.ddStopBits = QComboBox()
        self.ddStopBits.addItems(['1','1.5','2'])
        self.ddStopBits.setCurrentText('1')
        grid.addWidget(self.ddStopBits, 3, 1)
        
        grid.addWidget(QLabel("Parity:"), 4, 0)
        self.ddParity = QComboBox()
        self.ddParity.addItems(['none','even','odd','mark','space'])
        self.ddParity.setCurrentText('none')
        grid.addWidget(self.ddParity, 4, 1)
        
        self.btnConnect = QPushButton("Apply")
        self.btnConnect.clicked.connect(self.applySettings)
        grid.addWidget(self.btnConnect, 5, 0)
        left_layout.addLayout(grid)
        
        # Command input
        self.txt = QLineEdit()
        self.txt.setPlaceholderText("Please enter command or wavefreq to send.")
        left_layout.addWidget(self.txt)
        self.btnSend = QPushButton("Send")
        self.btnSend.clicked.connect(self.sendData)
        left_layout.addWidget(self.btnSend)
        
        self.btnSendWave = QPushButton("SendWave")
        self.btnSendWave.clicked.connect(self.sendDataWave)
        left_layout.addWidget(self.btnSendWave)
        
        # Start/Stop
        self.btnStartStop = QPushButton("Start Receiving")
        self.btnStartStop.clicked.connect(self.startStop)
        left_layout.addWidget(self.btnStartStop)
        
        self.lblStatus = QLabel("Status: Stopped")
        left_layout.addWidget(self.lblStatus)
        
        # Clear / Save
        self.btnClear = QPushButton("Clear Data")
        self.btnClear.clicked.connect(self.clearData)
        left_layout.addWidget(self.btnClear)
        self.btnSave = QPushButton("Save Now")
        self.btnSave.clicked.connect(self.saveData)
        left_layout.addWidget(self.btnSave)
        
        # Visibility checkboxes
        self.chk = []
        chk_layout = QGridLayout()
        for i, name in enumerate(self.colNames):
            cb = QCheckBox(name)
            cb.setChecked(True)
            cb.stateChanged.connect(self.updatePlots)
            self.chk.append(cb)
            row, col = divmod(i, 2)
            chk_layout.addWidget(cb, row, col)
        left_layout.addLayout(chk_layout)
        
        layout.addLayout(left_layout, 0)
        
        # Right: Plot area
        self.fig, self.ax = plt.subplots()
        self.canvas = FigureCanvas(self.fig)
        layout.addWidget(self.canvas, 1)
        
        # Setup plot lines
        self.plotLines = []
        colors = plt.cm.hsv(np.linspace(0, 1, self.NUMBER_))
        for i in range(self.NUMBER_):
            line, = self.ax.plot([], [], color=colors[i], label=self.colNames[i])
            self.plotLines.append(line)
        self.ax.legend(loc='upper right')
        self.ax.grid(True)
        self.ax.set_xlabel("Time (s)")
        self.ax.set_ylabel("Value")
        
    def refreshPorts(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.ddPort.clear()
        self.ddPort.addItems(ports)
    
    def applySettings(self):
        QMessageBox.information(self, "Info", '串口设置已保存，点击"开始接收"以打开连接。')
        
    def startStop(self):
        if not self.isRunning:
            port = self.ddPort.currentText()
            if not port:
                QMessageBox.warning(self, "Error", "未选择串口。")
                return
            baud = int(self.ddBaud.currentText())
            databits = int(self.ddDataBits.currentText())
            stopbits = float(self.ddStopBits.currentText())
            parity = self.ddParity.currentText()
            parity_map = {'none': serial.PARITY_NONE, 'even': serial.PARITY_EVEN, 
                          'odd': serial.PARITY_ODD, 'mark': serial.PARITY_MARK, 'space': serial.PARITY_SPACE}
            try:
                self.sp = serial.Serial(port, baudrate=baud, bytesize=databits,
                                        stopbits=stopbits, parity=parity_map[parity], timeout=1)
                # Windows 下可用
                self.sp.set_buffer_size(rx_size=40960, tx_size=2048)
                
            except Exception as e:
                QMessageBox.critical(self, "Error", f"打开串口失败: {e}")
                return
            self.isRunning = True
            self.btnStartStop.setText("Stop Receiving")
            self.lblStatus.setText(f"状态: 接收中 ({port} @ {baud})")
        else:
            try:
                if self.sp and self.sp.is_open:
                    self.sp.write(self.StopCommand)
                    self.sp.close()
            except Exception as e:
                print("Error stopping serial:", e)
            self.isRunning = False
            self.btnStartStop.setText("Start Receiving")
            self.lblStatus.setText("状态: 已停止")
            self.saveData()
    
    def sendData(self):
        if self.sp and self.sp.is_open:
            # stop data wave
            self.sintimer.stop()
            
            cmd = self.txt.text()
            print(cmd)
            if cmd:
                self.sp.write((cmd + '\r\n').encode('ascii'))
                # QMessageBox.information(self, "Info", f"已发送: {cmd}")
            else:
                QMessageBox.warning(self, "Warning", "请输入内容！")
    def sendDataWave(self):
        
        self.start_time = time.perf_counter()
        cmd = self.txt.text().strip()
        self.startrFromZero = 0
        try:
            self.Hz = float(cmd)
            print("这是数字:", self.Hz)
        except ValueError:
            print("这不是数字:")
            self.Hz = 1;     # default is 1Hz
            
        self.sintimer.start(10)  # 10 ms 周期
            
    
    def sendSin(self):
        if self.sp and self.sp.is_open:
            t = time.perf_counter()  - self.start_time
            value = 30*math.sin(2*math.pi*self.Hz*t) + 40        # unit second Hz
            if value < 15:
                self.startrFromZero = 1
            if self.startrFromZero == 1:
                data_str = f"{value:.5f}"   # 保留 6 位小数
                self.sp.write((data_str + '\r\n').encode('ascii'))
                print("Sent-", round(t,3))    # to check jitter
            
    def clearData(self):
        self.buf = np.empty((0, self.NUMBER_))
        self.times_arr = []
        self.dataTbl = pd.DataFrame(columns=self.colNames)
        for line in self.plotLines:
            line.set_data([], [])
            if hasattr(line, 'xdata'):
                line.xdata.clear()  # 或 line.xdata = []
            if hasattr(line, 'ydata'):
                line.ydata.clear()  # 或 line.ydata = []
        self.canvas.draw()
    
    def saveData(self):
        # you can save the data while collecting data
        if self.dataTbl.empty:
            return
        fname, _ = QFileDialog.getSaveFileName(self, "Save Data", f"serial_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv",
                                               "CSV files (*.csv);;All Files (*)")
        if fname:
            self.dataTbl.to_csv(fname, index=False)
            QMessageBox.information(self, "Info", f"Data saved to {fname}")
    
    def updatePlots(self):
        while self.isRunning == True and self.sp and self.sp.in_waiting  > 0:
            try:
                line = self.sp.readline().decode(errors='ignore').strip()
                if ',' in line:
                    parts = line.split(',')
                    parts = [p.strip() for p in parts if p.strip()]
                    if len(parts) >= self.NUMBER_:
                        nums = [float(p) for p in parts[:self.NUMBER_]]
                        # 更新缓冲区
                        self.buf = np.vstack([self.buf, nums])
                        t = nums[-1] / 1e6
                        self.times_arr.append(t)
                        # 更新数据表
                        self.dataTbl.loc[len(self.dataTbl)] = nums
                        

                        # ---- 逐点追加到曲线 ----
                        for i, plot_line in enumerate(self.plotLines):
                            if self.chk[i].isChecked():
                                # 初始化 xdata/ydata 列表
                                if not hasattr(plot_line, 'xdata'):
                                    plot_line.xdata = []
                                    plot_line.ydata = []
                                # 追加最新点
                                plot_line.xdata.append(t)
                                plot_line.ydata.append(nums[i])
                                # 可选：限制最大点数，实现滑动窗口
                                
                                plot_line.xdata = plot_line.xdata[-self.MAX_POINTS:]
                                plot_line.ydata = plot_line.ydata[-self.MAX_POINTS:]
                                # 更新曲线
                                plot_line.set_data(plot_line.xdata, plot_line.ydata)
                            else:
                                plot_line.set_data([], [])

            except Exception as e:
                print("updatePlots error:", e)
                pass
        
        if(self.isRunning == True):
            if len(self.buf) == 0:
                return

            # 自动缩放
            self.ax.relim()
            self.ax.autoscale_view()
            self.canvas.draw()

    
    def closeEvent(self, event):
        if self.isRunning:
            reply = QMessageBox.question(self, 'Exit', 
                                         "正在接收数据，关闭将停止并保存当前数据。确定要退出？", 
                                         QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
            if reply == QMessageBox.No:
                event.ignore()
                return
        self.clearData()
        if self.sp and self.sp.is_open:
            self.sp.close()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    gui = SerialGUI()
    gui.show()
    sys.exit(app.exec_())
