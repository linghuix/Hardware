function serialGUI
% serialGUI - simple receiving, sending, and drawing data from serial port in real time
% tested on MATLAB R2023 and R2022, requiring comma-separated data per row, default setting are ：
%     PackageID, float1, float2, float3, float4, float5, float6, float7, float8, MicroTimeStamp
%
% usage：
%   run in  MATLAB command. Select the serial port and parameters. Click button to start.

%% ---- default configuration setting, change as you want ----

% variable names. change name and number if you want
colNames = {'No','I','V','P','filtered_I','filtered_V','setpoint','cable_force','output','time'}; 
NUMBER_ = numel(colNames);   % number of data split with ',' and the last one is time microsecond

dataTbl = array2table(nan(0, NUMBER_), 'VariableNames', colNames);


%% ---- Global/Shared Variables ----
sp = [];                      % serialport object
isRunning = false;            % serial port running flag

% Save the data for plotting
times_arr = []; % store matlab datenums for x-axis (optional use of row time)
buf = [];      % numeric buffer Nx10


%% ---- GUI configuration ----
fig = uifigure('Name','Thunderbird-style Serial Port Data Processing GUI','Position',[200 200 1200 700]);
fig.CloseRequestFcn = @onClose;


% Left: Serial Port Settings Panel
panelCfg = uipanel(fig,'Title','Serial Port Settings','Position',[10 420 350 270]);

lblPort = uilabel(panelCfg,'Text','Port:','Position',[10 210 40 22]);
ddPort = uidropdown(panelCfg,'Items', serialportlist("available"), 'Position',[60 210 120 22]);
btnRefresh = uibutton(panelCfg,'Text','Refresh','Position',[190 210 60 22],'ButtonPushedFcn',@onRefreshPorts);

lblBaud = uilabel(panelCfg,'Text','Baud:','Position',[10 170 40 22]);
ddBaud = uidropdown(panelCfg,'Items',{'115200','128000','230400','256000','460800','921600'},'Value','115200','Position',[60 170 120 22]);

lblDataBits = uilabel(panelCfg,'Text','DataBits:','Position',[10 130 60 22]);
ddDataBits = uidropdown(panelCfg,'Items',{'7','8'},'Value','8','Position',[80 130 60 22]);
lblStopBits = uilabel(panelCfg,'Text','StopBits:','Position',[150 130 60 22]);
ddStopBits = uidropdown(panelCfg,'Items',{'1','1.5','2'},'Value','1','Position',[210 130 60 22]);
lblParity = uilabel(panelCfg,'Text','Parity:','Position',[10 90 50 22]);
ddParity = uidropdown(panelCfg,'Items',{'none','even','odd','mark','space'},'Value','none','Position',[60 90 100 22]);

btnConnect = uibutton(panelCfg,'Text','Apply','Position',[10 40 80 28],'ButtonPushedFcn',@onApplySettings);


txt = uieditfield(fig, "text", ...
    "Position", [10 280 350 40], ...
    "Placeholder", "Please enter command to send.");

% Send button
btn = uibutton(fig, "push", ...
    "Text", "Send", ...
    "Position", [10 230 50 30], ...
    "ButtonPushedFcn", @(btn,event) sendData(txt));
        

% Start/Stop button
btnStartStop = uibutton(fig,'Text','Start Receiving','Position',[10 350 100 40],'ButtonPushedFcn',@onStartStop);
lblStatus = uilabel(fig,'Text','Status: Stopped','Position',[10 390 300 30]);
%  Clear and Save buttons
btnClear = uibutton(fig,'Text','Clear Data','Position',[130 350 100 40],'ButtonPushedFcn',@onClear);
btnSaveNow = uibutton(fig,'Text','Save Now','Position',[250 350 100 40],'ButtonPushedFcn',@onSaveNow);


% Middle: Plot area
ax = uiaxes(fig,'Position',[380 60 800 600]);
title(ax,'实时参数曲线'); xlabel(ax,'Data Point #'); ylabel(ax,'Value');
grid(ax,'on'); hold(ax,'on');

% Create 10 animated lines (initially empty)
colors = hsv(NUMBER_);
										 
for k = 1:NUMBER_
    plotLines(k) = animatedline('Parent', ax, ...
                                'Color', colors(k,:), ...
                                'LineWidth', 1, ...
                                'MaximumNumPoints', 1000, ...
                                'DisplayName', colNames{k});
end
legend(ax,'show','Location','eastoutside');

% Left: Checkboxes to control visibility
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


% Info label
uilabel(fig,'Text','Expected data format: packageID, float1, float2, float3,... floatN, microTimeStamp','Position',[380 10 700 30]);

%% ---- Callbacks and Subfunctions ----

    function onRefreshPorts(~,~)
        try
            ddPort.Items = serialportlist("available");
        catch
            ddPort.Items = {};
        end
    end

    function onApplySettings(~,~)
        % Only save UI values. Actual serialport created on start.
        uialert(fig, '串口设置已保存，点击"开始接收"以打开连接。','提示');
    end

    function onStartStop(src,~)
        if ~isRunning
            % Start: create serialport,配置 callback
            try
                port = ddPort.Value;
                if isempty(port)
                    uialert(fig,'未选择串口。请在"Port"下拉中选择。','错误');
                    return;
                end
                baud = str2double(ddBaud.Value);
                databits = str2double(ddDataBits.Value);
                stopbits = str2double(ddStopBits.Value);
                parity = ddParity.Value;
                % Create serialport
                sp = serialport(port, baud, 'Timeout', 1, 'DataBits', databits, 'StopBits', stopbits, 'Parity', parity);
                configureCallback(sp, "terminator", @onSerialData); % called on newline
            catch ME
                uialert(fig, ['打开串口失败: ' ME.message],'错误');
                return;
            end
            isRunning = true;
            src.Text = '停止接收';
            lblStatus.Text = sprintf('状态: 接收中 (%s @ %d)', port, baud);
            % reset buffers
            buf = [];
            times_arr = [];
            dataTbl = dataTbl([], :); % empty
        else
            writeline(sp, 'p'); % stop command
            % Stop: delete callback and serialport, save data
            stopReceivingAndSave();
            src.Text = '开始接收';
            lblStatus.Text = '状态: 已停止';
        end
    end

    function onSerialData(src,~)
        % 该函数在收到 terminator（通常是 newline）时触发
        % 小心不要在此函数做太慢的操作（尽量只解析并存数据）
        try
            line = readline(src); % read one line text
        catch
            return;
        end
        if isempty(line); return; end
        % 解析一行
        % 允许前后空格
        line = strtrim(line);
        % 支持如果有其它前缀/后缀，则尝试提取包含逗号的部分
        if ~contains(line, ',')
            return;
        end
        % split and numeric
        parts = strsplit(line,',');
        % 有时最后有多余空项，删除空
        parts(cellfun(@(s) isempty(strtrim(s)), parts)) = [];
        if numel(parts) < NUMBER_
            % 忽略短行（也可记录错误计数）
            return;
        end
        % 取前10项并转数值
        nums = nan(1,NUMBER_);
        for k=1:NUMBER_
            val = strtrim(parts{k});
            % 尝试转为数字
            n = str2double(val);
            if isnan(n)
                % 如果不是数字，跳过整行
                return;
            end
            nums(k) = n;
        end
        % append to buffer
        buf(end+1,:) = nums; 	    %#ok
        times_arr(end+1) = nums(end)/1000000;   % last number is time microsecond
        % 更新 table（保留为数值型）
        newRow = array2table(nums, 'VariableNames', dataTbl.Properties.VariableNames);
        dataTbl = [dataTbl; newRow]; %#ok<AGROW>
        % 简单实时更新绘图（只绘最近 N 点以加速）
        updatePlots();
    end

    function updatePlots()
        if isempty(buf)
            return;
        end
		newData = buf(end,:);
        timestamp = times_arr(end);

        for k = 1:10
            if chk(k).Value
                addpoints(plotLines(k), timestamp, newData(k));
                plotLines(k).Visible = 'on';
            else
                plotLines(k).Visible = 'off';
            end
        end
        % 调整坐标轴
        try
            visibleCols = find([chk.Value]);   % 找出被勾选的列索引
            if ~isempty(visibleCols)
                % autoscale y limits with some margin
                allY = buf(idx, visibleCols);
                ymin = min(allY, [], 'all', 'omitnan');
                ymax = max(allY, [], 'all', 'omitnan');
                if isfinite(ymin) && isfinite(ymax) && (ymax>ymin)
                    yrange = ymax - ymin;
                    ylim(ax, [ymin - 0.1*yrange, ymax + 0.1*yrange]);
                end
            end
            xlim(ax, [min(x), max(x)]);
            drawnow limitrate;
        catch
            % ignore plotting errors
        end
    end

    function toggleLine(k)
        if chk(k).Value
            plotLines(k).Visible = 'on';
        else
            plotLines(k).Visible = 'off';
        end
    end

    function onClear(~,~)
        buf = [];
        times_arr = [];
        dataTbl = dataTbl([], :);
        for k=1:10
			clearpoints(plotLines(k));
        end
        drawnow;
    end

    function onSaveNow(~,~)
        saveDataToMat();
        uialert(fig,'当前数据已保存。','提示');
    end

    function stopReceivingAndSave()
        % stop callback and delete serialport safely
        try
            if ~isempty(sp) && isvalid(sp)
                configureCallback(sp, "off");
                delete(sp);
                sp = [];
            end
        catch
            % ignore
        end
        isRunning = false;
        % 保存数据
        saveDataToMat();
    end

    function saveDataToMat()
        if isempty(dataTbl) || height(dataTbl)==0
            % nothing to save
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

        
        % 保存变量 dataTbl、buf、times_arr 以便后续分析
        try
            save(fname, 'dataTbl', 'buf', 'times_arr', '-v7.3');
        catch
            % 若 v7.3 出问题，尝试不指定
            save(fname, 'dataTbl', 'buf', 'times_arr');
        end
        disp(['已保存数据到: ' fullfile(pwd, fname)]);
    end
    
    % 按钮回调：发送数据
    function sendData(txt)
        data = txt.Value;
        if ~isempty(data)
            writeline(sp, data);  % 发送字符串并自动加换行符
            uialert(txt.Parent, "已发送: " + data, "提示");
        else
            uialert(txt.Parent, "请输入内容！", "警告");
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
                if ~isempty(sp) && isvalid(sp)
                    configureCallback(sp, "off");
                    delete(sp);
                end
            catch
            end
        end
        delete(fig);
    end

end
