 
% delete(gcp('nocreate'))

% Start parallel pool if not already running
if isempty(gcp('nocreate'))
    parpool(4);
end

% Shared variables using DataQueue
encoderQueue = parallel.pool.DataQueue;
forceQueue = parallel.pool.DataQueue;
encoderLeftQueue = parallel.pool.DataQueue;

% Buffers for synchronized data
encoder_data = []; % [timestamp, angle]
force_data = [];   % [timestamp, force]
encoder_data_left = [];   % [timestamp, force]

% Parameters
buffer_size = 100;      % Buffer size for storing latest data
sync_threshold = 0.1;   % Acceptable time difference (in seconds)

time_base = posixtime(datetime('now'));

disp('start');

% A dialog to stop the loop
% MessageBox = msgbox( 'Stop DataStream Client', 'Vicon DataStream SDK' );


% Start background tasks for data acquisition
f1 = parfeval(@ReceiveEncoderData, 0, encoderQueue, time_base);
f2 = parfeval(@ReceiveViconData, 0, forceQueue, time_base);
f3 = parfeval(@ReceiveEncoderData_left, 0, encoderLeftQueue, time_base);

% Callback functions to update buffers
afterEach(encoderQueue, @(data) updateBuffer('encoder', data));
afterEach(forceQueue, @(data) updateBuffer('force', data));
afterEach(encoderLeftQueue, @(data) updateBuffer('encoderLeft', data));

% store data
datapoint = 3600*2*100;
syncData = nan(datapoint, 8);
storeIndex = 1;

% TBE estimation
tbe.previous_Period_List = [];
tbe.estimatedPeriod = 0;
tbe.lasteventTime = -10;
tbe.laststrikeTime = -10;
tbe.eventcounter = 0;
tbe.preN = 1;

% norminal abduction torque
tbe.tor =  load('norminal_tor.mat');

% intial value
GP = 0; duration = 0;
assist_torque = 0; assist_force = 0;

synced_angle=0;
synced_force=0;
GP=0;
duration=0;
assist_torque=0;

disp('Loop...');
% Main synchronization loop
% while ishandle( MessageBox )

LastTime = 0;
while true

    % Check for stop file
    if isfile('stop.flag')
        disp('Stop flag detected. Exiting...');
        break;
    end

    if ~isempty(encoder_data) && ~isempty(force_data) && ~isempty(encoder_data_left)
	
		syn_flag = 0; 		% synchronized if 1

        % Get latest force data / timestamps
        encoder_time  = encoder_data(end, 1);
        encoder_value = encoder_data(end, 2);
		if(encoder_time == LastTime)
%             disp('same')
			pause(0.001); % Adjust as needed
			continue;
		end
		LastTime = encoder_time;
		% disp([vicon_time, force_value])

        % Find closest encoder timestamp
        [difftime1, idx1] = min(abs(force_data(:, 1) - encoder_time));
        [difftime2, idx2] = min(abs(encoder_data_left(:, 1) - encoder_time));

        if difftime1 < sync_threshold && difftime2 < sync_threshold && size(force_data, 1) >= 2 
			syn_flag = 1;
			
            synced_force = force_data(idx1, 2);
            synced_angle_left = encoder_data_left(idx2, 2);
%             synced_force = interp1(force_data(:,1), force_data(:,2), encoder_time, 'linear', 'extrap');
            synced_angle = encoder_value;

            % Compute assistance
            [tbe, GP, duration, assist_torque, assist_force] = ComputeAssistance(tbe, encoder_time, synced_angle, synced_force);

        end
		
		% Display result
		fprintf('%d|T:%4.3f|A:%.1f|F:%.1f|P:%.1f|C:%.1f|t:%.1f|A2:%.1f\n', ...
				syn_flag, encoder_time, synced_angle, synced_force, GP, duration, assist_torque, synced_angle_left);

        syncData(storeIndex,:) = [syn_flag, encoder_time, synced_angle, synced_force, GP, tbe.estimatedPeriod, assist_torque, assist_force];
        storeIndex = storeIndex+1;

    end
    pause(0.001); % Adjust as needed
end


disp('Stop');


% Cancel background tasks if they exist and are cancellable
if exist('f1', 'var') && isa(f1, 'parallel.FevalFuture') && isvalid(f1) && ~strcmp(f1.State, 'finished')
    disp('取消任务 f1...');
    cancel(f1);
else
    disp('f1 无需取消（可能已完成或不存在）');
end

if exist('f2', 'var') && isa(f2, 'parallel.FevalFuture') && isvalid(f2) && ~strcmp(f2.State, 'finished')
    disp('取消任务 f2...');
    cancel(f2);
else
    disp('f2 无需取消（可能已完成或不存在）');
end

if exist('f3', 'var') && isa(f3, 'parallel.FevalFuture') && isvalid(f3) && ~strcmp(f3.State, 'finished')
    disp('取消任务 f3...');
    cancel(f3);
else
    disp('f3 无需取消（可能已完成或不存在）');
end

% Save the variable to a .mat file
save('syncData.mat', 'syncData');

	

% Clean up parallel pool
%poolobj = gcp('nocreate');
%if ~isempty(poolobj)
%    delete(poolobj);
%end
%disp('Clean up parallel pool');


% Stop parallel execution when done

%% Function to receive encoder data (runs in background) using pc time
function ReceiveEncoderData_2(queue, time_base)

    server_port = 12345;  % Must match the Arduino port

    % Create a TCP/IP server
    server = tcpserver("0.0.0.0", server_port, "ConnectionChangedFcn", @(s,e)disp("Client Connected"));
    
    disp("Waiting for Arduino to connect...");
    
    % Wait until Arduino connects
    while ~server.Connected
        pause(0.1);
    end
    disp("Arduino connected!");
    
    % Send "SEND_DATA" command to Arduino
    writeline(server, 'SEND_DATA');
    disp("Command 'SEND_DATA' sent to Arduino.");
    
    % 增加超时检测, Continuously receive and process data
    max_idle_time = 5; % 5 seconds timeout
    last_receive_time = posixtime(datetime('now'));

    while true

        [PCtime, rawData] = readUnitil(server, time_base);
        [t, rawData] = strtok(rawData, ",");

        last_receive_time = PCtime; % 更新最后接收时间
        filteredData = str2double(rawData); % Convert and remove '@'

        if ~isnan(filteredData)  % Check if data is valid
            encoder_time = PCtime;
            angle_value = filteredData; % Store data
            send(queue, [encoder_time, angle_value]);
        end

        pause(0.001);
    end
    warning('Encoder does not response in %d seconds', max_idle_time);
end

%% Function to receive encoder data (runs in background) sync with esp32 time
function ReceiveEncoderData(queue, time_base)

    server_port = 12345;  % Must match the Arduino port

    % Create a TCP/IP server
    server = tcpserver("0.0.0.0", server_port, "ConnectionChangedFcn", @(s,e)disp("Client Connected"));
    
    disp("Waiting for Arduino to connect...");
    
    % Wait until Arduino connects
    while ~server.Connected
        pause(0.1);
    end
    disp("Arduino connected!");
    
    % Send "SEND_DATA" command to Arduino
    writeline(server, 'SEND_DATA');
    disp("Command 'SEND_DATA' sent to Arduino.");
    
    % 增加超时检测, Continuously receive and process data
    max_idle_time = 5; % 5 seconds timeout
    last_receive_time = posixtime(datetime('now'));

    syncESP = [];syncPC  = [];
    while true

        [PCtime, rawData] = readUnitil(server, time_base);
        [ESP_tms, rawData] = strtok(rawData, ",");
        ESP_tms = str2double(ESP_tms);

        if isempty(syncESP)
            % First packet — use for synchronization
            syncESP = ESP_tms;
            syncPC = PCtime;
        end

%         Convert to PC time
        dt = ((ESP_tms - syncESP) / 1000); % second
        PC_ESPtime = syncPC + dt;

        last_receive_time = PC_ESPtime; % 更新接收时间
        filteredData = str2double(rawData); % Convert and remove '@'

        if ~isnan(filteredData)  % Check if data is valid
            encoder_time = last_receive_time;
            angle_value = filteredData; % Store data
            send(queue, [encoder_time, angle_value]);
        end

        pause(0.001);
    end
    warning('Encoder does not response in %d seconds', max_idle_time);
end


%% Function to receive encoder data (runs in background) sync with esp32 time
function ReceiveEncoderData_left(queue, time_base)

    server_port = 12346;  % Must match the Arduino port

    % Create a TCP/IP server
    server = tcpserver("0.0.0.0", server_port, "ConnectionChangedFcn", @(s,e)disp("Client Connected"));
    
    disp("Waiting for Arduino to connect...");
    
    % Wait until Arduino connects
    while ~server.Connected
        pause(0.1);
    end
    disp("Arduino connected!");
    
    % Send "SEND_DATA" command to Arduino
    writeline(server, 'SEND_DATA');
    disp("Command 'SEND_DATA' sent to Arduino.");
    
    % 增加超时检测, Continuously receive and process data
    max_idle_time = 5; % 5 seconds timeout
    last_receive_time = posixtime(datetime('now'));

    syncESP = [];syncPC  = [];
    while true

        [PCtime, rawData] = readUnitil(server, time_base);
        [ESP_tms, rawData] = strtok(rawData, ",");
        ESP_tms = str2double(ESP_tms);

        if isempty(syncESP)
            % First packet — use for synchronization
            syncESP = ESP_tms;
            syncPC = PCtime;
        end

%         Convert to PC time
        dt = ((ESP_tms - syncESP) / 1000); % second
        PC_ESPtime = syncPC + dt;

        last_receive_time = PC_ESPtime; % 更新接收时间
        filteredData = str2double(rawData); % Convert and remove '@'

        if ~isnan(filteredData)  % Check if data is valid
            encoder_time = last_receive_time;
            angle_value = filteredData; % Store data
            send(queue, [encoder_time, angle_value]);
        end

        pause(0.001);
    end
    warning('Encoder does not response in %d seconds', max_idle_time);
end

%% Function to receive Vicon force data (runs in background)
function ReceiveViconData1(queue, time_base)

    % A dialog to stop the loop
%    MessageBox = msgbox( 'Stop DataStream Client', 'Vicon DataStream SDK' );
    
    % Load the SDK
    fprintf( 'Loading SDK...' );
    addpath( 'C:\Program Files\Vicon\DataStream SDK\Win64\dotNET' );
    dssdkAssembly = which('ViconDataStreamSDK_DotNET.dll');
    if dssdkAssembly == ""
      [ file, path ] = uigetfile( '*.dll' );
      if isequal( file, 0 )
        fprintf( 'User canceled' );
        return;
      else
        dssdkAssembly = fullfile( path, file );
      end   
    end
    
    NET.addAssembly(dssdkAssembly);
    fprintf( 'done\n' );
    
    % Program options
    HostName = '130.237.233.178:801';
    
    % Make a new client
    MyClient = ViconDataStreamSDK.DotNET.Client();
    
    % Connect to a server
    fprintf( 'Connecting to %s ...', HostName );
    while ~MyClient.IsConnected().Connected
      % Direct connection
      MyClient.Connect( HostName );
      fprintf( '.' );
    end
    fprintf( '\n' );
    
    MyClient.EnableDeviceData();
    fprintf( 'Device Data Enabled: %d\n', MyClient.IsDeviceDataEnabled().Enabled );
    
    % Set the streaming mode
    MyClient.SetStreamMode( ViconDataStreamSDK.DotNET.StreamMode.ClientPull );
    % MyClient.SetStreamMode( StreamMode.ClientPullPreFetch );
    % MyClient.SetStreamMode( StreamMode.ServerPush );
    
    % Set the global up axis
    MyClient.SetAxisMapping( ViconDataStreamSDK.DotNET.Direction.Forward, ...
                             ViconDataStreamSDK.DotNET.Direction.Left,    ...
                             ViconDataStreamSDK.DotNET.Direction.Up );    % Z-up
    % MyClient.SetAxisMapping( Direction.Forward, ...
    %                          Direction.Up,      ...
    %                          Direction.Right ); % Y-up
    
    Output_GetAxisMapping = MyClient.GetAxisMapping();
    fprintf( 'Axis Mapping: X-%s Y-%s Z-%s\n', Output_GetAxisMapping.XAxis.ToString(), ...
                                               Output_GetAxisMapping.YAxis.ToString(), ...
                                               Output_GetAxisMapping.ZAxis.ToString() );
    
    
    % Discover the version number
    Output_GetVersion = MyClient.GetVersion();
    fprintf( 'Version: %d.%d.%d\n', Output_GetVersion.Major, ...
                                    Output_GetVersion.Minor, ...
                                    Output_GetVersion.Point );
      
      
    % Loop until the message box is dismissed

    % 增加超时检测, Continuously receive and process data
    max_idle_time = 5; % 5 seconds timeout
    last_receive_time = posixtime(datetime('now'));

    while (posixtime(datetime('now')) - last_receive_time) < max_idle_time
        
      % Get a frame
      while MyClient.GetFrame().Result ~= ViconDataStreamSDK.DotNET.Result.Success
      end
    
      % Get the frame number
      Output_GetFrameNumber = MyClient.GetFrameNumber();
    %   fprintf( 'Frame Number: %d\n', Output_GetFrameNumber.FrameNumber );
    
      % Get the timecode
      Output_GetTimecode = MyClient.GetTimecode();
    %   fprintf( 'Timecode: %dh %dm %ds %df %dsf %d %s %d %d\n\n', ...
    %                      Output_GetTimecode.Hours,               ...
    %                      Output_GetTimecode.Minutes,             ...
    %                      Output_GetTimecode.Seconds,             ...
    %                      Output_GetTimecode.Frames,              ...
    %                      Output_GetTimecode.SubFrame,            ...
    %                      Output_GetTimecode.FieldFlag,           ...
    %                      Output_GetTimecode.Standard.ToString(), ...
    %                      Output_GetTimecode.SubFramesPerFrame,   ...
    %                      Output_GetTimecode.UserBits );
    
      % Get the latency
      fprintf( 'Latency: %gs\n', MyClient.GetLatencyTotal().Total );
      for LatencySampleIndex = 1:MyClient.GetLatencySampleCount().Count
        SampleName  = MyClient.GetLatencySampleName( LatencySampleIndex ).Name;
        SampleValue = MyClient.GetLatencySampleValue( SampleName ).Value;
        fprintf( '  %s %gs\n', SampleName, SampleValue );
      end% for  
      fprintf( '\n' );
                         
    
      DeviceCount = MyClient.GetDeviceCount().DeviceCount;
      % Get the device name and type
      Output_GetDeviceName = MyClient.GetDeviceName( 1 );    
        
      % Count the number of device outputs
      DeviceOutputCount = MyClient.GetDeviceOutputCount( Output_GetDeviceName.DeviceName ).DeviceOutputCount;
      for DeviceOutputIndex = 1:DeviceOutputCount
          % Get the device output name and unit
          Output_GetDeviceOutputName = MyClient.GetDeviceOutputName( Output_GetDeviceName.DeviceName, DeviceOutputIndex );
          if DeviceOutputIndex == 12
            % Get the device output value
            Output_GetDeviceOutputValue = MyClient.GetDeviceOutputValue( Output_GetDeviceName.DeviceName, Output_GetDeviceOutputName.DeviceOutputName );
    %         ForceData.Force(end+1) = Output_GetDeviceOutputValue.Value;
            
            last_receive_time = posixtime(datetime('now'));
            time = last_receive_time-time_base; 
            force_value = Output_GetDeviceOutputValue.Value; % FswA
            send(queue, [time, force_value]);
    
          end
      end   % DeviceOutputIndex
    end     % while true  
    
    warning('Vicon does not response in %d seconds', max_idle_time);

    % Disconnect and dispose
    MyClient.Disconnect();
    
    % Unload the SDK
    fprintf( 'Unloading SDK...' );
    Client.UnloadViconDataStreamSDK();
    fprintf( 'done\n' );
end

%% Function to receive Vicon force data (runs in background)
function ReceiveViconData(queue, time_base)

    while true
        time = posixtime(datetime('now')) - time_base; % Replace with actual timestamp from network
        force_value = time; % Replace with actual sensor data
        send(queue, [time, force_value]);
        pause(0.001); % Simulating sensor frequency
    end
end

%% Function to extract force data from Vicon
function force = GetForceData(Client)
    ForcePlateCount = Client.GetForcePlateCount().ForcePlateCount;
    force = 0;
    for i = 1:ForcePlateCount
        ForceVector = Client.GetGlobalForceVector(i);
        force = force + ForceVector.ForceVector(3); % Summing Z-axis forces
    end
end

%% Function to update buffers
function updateBuffer2(type, data)
    persistent encoder_data force_data encoderQueue forceQueue

    if isempty(encoder_data), encoder_data = []; end
    if isempty(force_data), force_data = []; end

    % 确保队列存在
    if isempty(encoderQueue) || isempty(forceQueue)
        return;
    end

    % Ensure buffer doesn't grow indefinitely
    if strcmp(type, 'encoder')
        encoder_data = [encoder_data; data];
        if size(encoder_data, 1) > 100
            encoder_data(1:(size(encoder_data, 1) - 100), :) = [];
        end

        % 清理 DataQueue 以防数据堆积
        %flushDataQueue(encoderQueue);

    elseif strcmp(type, 'force')
        force_data = [force_data; data];

        if size(force_data, 1) > 100
            force_data(1:(size(force_data, 1) - 100), :) = [];
        end
        
        %flushDataQueue(forceQueue);
    end

    % 更新到 workspace 变量，方便调试
    assignin('base', 'encoder_data', encoder_data);
    assignin('base', 'force_data'  , force_data);
end

%% Function to update buffers
function updateBuffer(type, data)

    persistent encoder_data force_data encoder_data_left
    if isempty(encoder_data), encoder_data = []; end
    if isempty(force_data), force_data = []; end
    if isempty(encoder_data_left), encoder_data_left = []; end

    if strcmp(type, 'encoder')
        encoder_data = [encoder_data; data];
        if size(encoder_data, 1) > 100, encoder_data(1, :) = []; end
    elseif strcmp(type, 'force')
        force_data = [force_data; data];
        if size(force_data, 1) > 100, force_data(1, :) = []; end
    elseif strcmp(type, 'encoderLeft')
        encoder_data_left = [encoder_data_left; data];
        if size(encoder_data_left, 1) > 100, encoder_data_left(1, :) = []; end
    end

    % Ensure buffer doesn't grow indefinitely
    assignin('base', 'encoder_data', encoder_data);
    assignin('base', 'force_data', force_data);
    assignin('base', 'encoder_data_left', encoder_data_left);
end

%% Function to compute assistance
function [tbe, assist, duration_strike, assist_torque, assist_force] = ComputeAssistance(tbe, vicon_time, angle, force)

    % % This method takes an event and time_index to estimate gait cycle
    event = force > 1;  % 1 heel strike occurs
    duration_event = vicon_time - tbe.lasteventTime;
    duration_strike = vicon_time - tbe.laststrikeTime;


    % % make sure continous event only record once
    if ( event && duration_event > 0.2) % 0.5 second

        tbe.laststrikeTime = vicon_time; % considered as a heel strike

        % % Cycle if too large, not likely a step
        if (duration_event > 0.2 && duration_strike < 3 && duration_strike > 0.5) 
            tbe.previous_Period_List = [tbe.previous_Period_List, duration_strike];
        end

        % record event time
        tbe.eventcounter = tbe.eventcounter + 1;

        % keep previous_Period_List length
        if length(tbe.previous_Period_List) > tbe.preN
            tbe.previous_Period_List(1) = [];
        end

        % estimate gait cycle from previous gait cycles
        if isempty(tbe.previous_Period_List)
            tbe.estimatedPeriod = 0;
        else
            tbe.estimatedPeriod = mean(tbe.previous_Period_List);
        end

%         GaitPhase = 0;
    end

    if (event)
        tbe.lasteventTime = vicon_time;
    end

    duration_strike = vicon_time - tbe.laststrikeTime;
    if (tbe.estimatedPeriod == 0)
         GaitPhase = 0;
    elseif (duration_strike > tbe.estimatedPeriod)
         GaitPhase = 99;
    else
        GaitPhase = duration_strike / tbe.estimatedPeriod * 100;
    end


%     GaitPhase = 0;
    assist = GaitPhase; % Example formula

    % % assistive torque  gait phase => 
    assist_torque = interp1(0:99, tbe.tor.hipAddTor, GaitPhase, 'pchip', 'extrap');

    % % assistive force
    % rotation matrix
    angle = angle/180*pi;
    R = [cos(angle), -sin(angle);
         sin(angle),  cos(angle)];
    
    % define point (x, y)
    a=0.1;b = 0.1;c = 0.04;d = 0.1;
    
    A = [0; a];
    B = [b; 0]; 
    C = [b+c; 0]; 
    D = [b+c; d];

    % rotated_point = R * point;
    % Define the vector (line) through point A with direction v
    DD = R*[c;d] + [b;0];       % Point on the vector B
    v = DD - A;                 % Direction vector (vx, vy)
    
    % Define the point P
    P = B; 
    
    % Calculate the distance
    numerator = abs(v(2) * (P(1) - A(1)) - v(1) * (P(2) - A(2)));
    denominator = sqrt(v(1)^2 + v(2)^2);
    momentArm = numerator / denominator;

    assist_force = assist_torque/momentArm;

end

%% Function to write data from data stream
function [PCtime, data] = readUnitil(server, time_base)
    data = '';
    while true
        charReceived = char(read(server, 1, "uint8")); % Read one character at a time
        if charReceived == "@"
            PCtime = posixtime(datetime('now')) - time_base;

            break; % Stop reading when '@' is found
        end
        data = [data, charReceived]; % Append character to string
    end
end

% Read multiple character at a time
function [time, data] = readUnitil_2(server, time_base)
    persistent buffer
    if isempty(buffer)
        buffer = '';
    end

    data = '';

    % Read all available bytes at once
    if server.NumBytesAvailable > 0
        time = posixtime(datetime('now')) - time_base;
        newData = read(server, server.NumBytesAvailable, "char");
        % buffer = newData;
        buffer = [buffer, newData];
    
        % Look for the terminator character
        idx = strfind(buffer, '@');
    
        if ~isempty(idx)
            if(idx(end) > 1)
    
                % if only one '@' is detected
                if(size(idx,2) == 1 && idx(end)>1)
                    % disp('detect one @')
                    data = strtrim(buffer( 1:(idx(end)-1) ));  % message content
                end
    
                % if multiple '@' is detected, choose the latest one
                if(size(idx,2) > 1)
                    % disp('detect multiple @')
                    data = strtrim(buffer( (idx(end-1)+1):(idx(end)-1) ));  % message content
                end
    
                buffer = buffer( (idx(end)+1):end );  % Keep remaining buffer after the terminator
            end
        end
    end
end