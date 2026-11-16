% 数据类型 struct {
  % uint32_t packageID;
  % float current_mA;
  % float speed_dps;
  % float position_deg;
  % float filtered_cur;
  % float filtered_sp;
  % float setpoint;
  % float measured_f;
  % float output;
  % uint32_t timeRecord;
% };


function udpGUI
% serialGUI - 简单的串口数据接收与实时绘图 GUI
% 适用于 MATLAB R2023，要求数据每行逗号分隔，10列：
% No,I,V,P,filtered_I,filtered_V,setpoint,cable_force,output,time
%
% 使用：
%   save 为 serialGUI.m，然后在 MATLAB 命令行运行 serialGUI

%% ---- configuration setting ----

DEFAULT_RX_PORT = 6000;   % 本地接收端口
DEFAULT_TX_PORT = 6001;   % 默认发送端口

MAX_POINTS_DEFAULT = 1000;

PACKET_BYTES = 40;
colNames = {'packageID','currentmA','speedDps','positionDeg','filteredCur','filteredSp','setpoint','measuredF','output','timeRecord'}; % 列信息
NUMBER_ = numel(colNames);   % number of data split with ',' and the last one is time microsecond

% Pre allocated array
Nmax = 100*60*60*24; % max package number
DataArray = nan(Nmax, NUMBER_);
writeIdx = 0;

% 数据存储
dataStore = struct();
for i=1:NUMBER_
    dataStore.(colNames{i}) = [];
end
dataStore.tstamp = [];

%% ---- status states ----
isRunning = false;            % 是否正在接收
% udp
rxUdp = [];
txUdp = [];

%% ---- GUI 布局 ----
fig = uifigure('Name','Thunderbird-style UDP数据整理 GUI','Position',[200 200 1200 700]);
fig.CloseRequestFcn = @onClose;


% udp setting GUI
panelCfg = uipanel(fig,'Title','udp setting','Position',[10 420 350 270]);

uilabel(panelCfg,'text','Send to IP:','Position',[10 210 60 22]);
editTxIP = uieditfield(panelCfg,'Text','Position',[80 210 100 22],'Placeholder','127.0.0.1');
editTxPort = uieditfield(panelCfg,'Text','Position',[190 210 60 22], 'Placeholder',num2str(DEFAULT_TX_PORT));

uilabel(panelCfg,'Text','Listen port (RX):','Position',[10 170 100 22]);
editRxPort = uieditfield(panelCfg, 'Text', 'Position',[120 170 80 22], 'Placeholder',num2str(DEFAULT_RX_PORT));


% 文本框
txt = uieditfield(fig, "text", ...
    "Position", [10 280 350 40], ...
    "Placeholder", "请输入要发送的UDP命令");

% 发送按钮
btn = uibutton(fig, "push", ...
    "Text", "发送", ...
    "Position", [10 230 50 30], ...
    "ButtonPushedFcn", @(btn,event) sendData(txt));
        

% Start/Stop button
% btnStartStop = uibutton(fig,'Text','开始接收','Position',[10 350 100 40],'ButtonPushedFcn',@onStartStop);
btnStartStop = uibutton(fig,'Text','Start (Listen)','Position',[10 350 100 40],'ButtonPushedFcn',@onStartStop);
    
lblStatus = uilabel(fig,'Text','状态: 已停止','Position',[10 390 300 30]);
% 清空、导出示例按钮
btnClear = uibutton(fig,'Text','清空数据','Position',[130 350 100 40],'ButtonPushedFcn',@onClear);
btnSaveNow = uibutton(fig,'Text','立即保存','Position',[250 350 100 40],'ButtonPushedFcn',@onSaveNow);


% Drawing area
ax = uiaxes(fig,'Position',[380 60 800 600]);
title(ax,'实时参数曲线'); xlabel(ax,'Data Point #'); ylabel(ax,'Value');
grid(ax,'on'); hold(ax,'on');

% Line object
colors = hsv(NUMBER_);
										 
for k = 1:NUMBER_
    plotLines(k) = animatedline('Parent', ax, ...
                                'Color', colors(k,:), ...
                                'LineWidth', 1, ...
                                'MaximumNumPoints', MAX_POINTS_DEFAULT, ...
                                'DisplayName', colNames{k}); % 最多保留点数
end
legend(ax,'show','Location','eastoutside');

% select box GUI
panelChk = uipanel(fig,'Title','显示控制','Position',[10 10 350 200]);
chk = gobjects(10,1);
for k = 1:ceil(NUMBER_/2)
    r = 10 - k; % arrange from top to bottom
    chk(k) = uicheckbox(panelChk,'Text',colNames{k}, 'Value', true, ...
        'Position',[10 150 - (k-1)*16 140 20], 'ValueChangedFcn', @(s,e) toggleLine(k));
end
for k = (ceil(NUMBER_/2)+1):NUMBER_
    r = 10 - k; % arrange from top to bottom
    chk(k) = uicheckbox(panelChk,'Text',colNames{k}, 'Value', true, ...
        'Position',[160 150 - (k-(ceil(NUMBER_/2)+1))*16 140 20], 'ValueChangedFcn', @(s,e) toggleLine(k));
end


% 说明标签
uilabel(fig,'Text','接收示例格式: No,I,V,P,filtered_I,filtered_V,setpoint,cable_force,output,time','Position',[380 10 700 30]);

%% ---- 回调与子函数 ----

    function onStartStop(src,~)
        if ~isRunning
            % Start: create serialport,配置 callback
            try
                rxPort = str2double(editRxPort.Value);
                rxUdp = udpport("datagram","LocalPort",rxPort,"Timeout",0.1);
                rxUdp.InputBufferSize = 65535;
                configureCallback(rxUdp,"datagram",1,@onSerialData);            % trigger callback each package
                isRunning = true;
				writeIdx = 0;
                src.Text = 'Stop Listening';
				lblStatus.Text = sprintf('状态: 接收中 (port %d)', rxPort);
            catch ME
                uialert(fig, ['无法创建 UDP 接收端: ' ME.message],'Error');
                isRunning = false;
            end
        else
            stopReceivingAndSave();
            src.Text = 'Start Listening';
            lblStatus.Text = '状态: 已停止';
        end
    end

    function onSerialData(src,~)
    
        while src.NumDatagramsAvailable > 0
            frame = read(src, 1, "uint8");   % read 1 datagram (return bytes)
            b = uint8(frame.Data(:));

            % 确保包长度正确
            if numel(b) ~= PACKET_BYTES
                disp("Invalid packet length: " + numel(b));
                 continue;
            end

            % 解析数据
            packageID = typecast(b(1:4),'uint32');
            floats = typecast(b(5:(PACKET_BYTES-4)),'single');% 8 个 float
            timeRecord = typecast(b((PACKET_BYTES-3):PACKET_BYTES),'uint32');
            nums =  [double(packageID), floats(:)', double(timeRecord)];  % if no double(), floats will be integer
			
			writeIdx = writeIdx + 1;
			if writeIdx <= Nmax
				DataArray(writeIdx,:) = nums;
			else
				warning('Data buffer full, dropping packet');
			end
            
            % append to buffer
            buf = nums;
            times_arr = nums(end)/1000000.00;   % last number is time microsecond

            updatePlots(buf, times_arr);
        end
    end

    function updatePlots(buf, tsec)
	
		newData = double(buf);
        timestamp = double(tsec);  % addpoints require double

        for k = 1:10
            if chk(k).Value
                addpoints(plotLines(k), timestamp, newData(k));
                plotLines(k).Visible = 'on';
            else
                plotLines(k).Visible = 'off';
            end
        end
        % 自动调整坐标轴
		drawnow limitrate;
    end

    function toggleLine(k)
        if chk(k).Value
            plotLines(k).Visible = 'on';
        else
            plotLines(k).Visible = 'off';
        end
    end

    function onClear(~,~)

        for k = 1:NUMBER_
			clearpoints(plotLines(k));
        end
        DataArray(:,:) = nan;
		writeIdx = 0;
        drawnow;
    end

    function onSaveNow(~,~)
        saveDataToMat();
        uialert(fig,'当前数据已保存。','提示');
    end

    function stopReceivingAndSave()
        % stop callback and delete serialport safely
        try
            if ~isempty(rxUdp) && isvalid(rxUdp)
                configureCallback(rxUdp, "off");
                delete(rxUdp);
                rxUdp = [];
            end
        catch
            % ignore
        end
        isRunning = false;
        % 保存数据
        saveDataToMat();
    end

    function saveDataToMat()
        if writeIdx == 0
          disp('No data to save.');
          return;
        end

        % Prompt the user to enter a file name (without extension)
        answer = inputdlg('Enter file name (without extension):', 'Save Data', [1 50], {'serial_data'});
        if isempty(answer)
            % User cancelled the input
            return;
        end

        fileName = answer{1};
        ts = datestr(now,'yyyymmdd_HHMMSS');
        if isempty(fileName)
            fname = sprintf('%s_serial_data.mat', ts);
        else
            fname = [ts '_' fileName '.mat'];
        end
		
        validData = DataArray(1:writeIdx,:);
        dataTbl = array2table(validData, 'VariableNames', colNames);
		
        % 保存变量 dataTbl、buf、times_arr 以便后续分析
        try
            save(fname, 'dataTbl','-v7.3');
        catch
            % 若 v7.3 出问题，尝试不指定
            save(fname, 'dataTbl');
        end
        disp(['Saved data to: ' fullfile(pwd, fname)]);
    end
    
    % 按钮回调：发送数据
    function sendData(txt)
        % data = txt.Value; 
        % if ~isempty(data)
            % writeline(sp, data);  % 发送字符串并自动加换行符
            % uialert(txt.Parent, "已发送: " + data, "提示");
        % else
            % uialert(txt.Parent, "请输入内容！", "警告");
        % end
        
        
        ip = editTxIP.Value   % string
        port = str2double(editTxPort.Value)
        
        if isempty(txUdp) || ~isvalid(txUdp)
            txUdp = udpport("datagram","Timeout",1);
        end
        msg = txt.Value;
		
		try
			write(txUdp, uint8(msg), ip, port);
		catch ME
			uialert(fig, ['发送失败: ' ME.message],'Error');
		end
    end

    function onClose(~,~)
        % 关闭前确保串口资源释放及自动保存
        if isRunning
            selection = questdlg('正在接收数据，关闭将停止并保存当前数据。确定要退出？','退出确认','是','否','否');
            if strcmp(selection,'否')
                return;
            end
            stopReceivingAndSave();
        else
            % 无运行时仍尝试释放可能存在的串口对象
            try
                if ~isempty(rxUdp) && isvalid(rxUdp)
                    configureCallback(rxUdp, "off");
                    delete(rxUdp);
                end
				if ~isempty(txUdp) && isvalid(txUdp)
					delete(txUdp);
				end
            catch
            end
        end
        delete(fig);
    end

end


