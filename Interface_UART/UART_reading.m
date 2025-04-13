
% This script connects to a Bluetooth or serial device (e.g., an Arduino) and 
% continuously reads sensor data, which is then plotted in real-time. The data 
% is expected to be in the format of a string with five elements, where the first 
% element is a character ('a') followed by four numeric values. The script allows 
% for real-time monitoring of sensor readings over a specified number of iterations, 
% with the option to display a limited number of points in the plot.

% Key Functionality:
% 1. Establishes a serial connection to a device via a specified COM port and baud rate.
% 2. Configures the communication to use Carriage Return/Line Feed (CR/LF) as the terminator.
% 3. Sends a command ('c') to the device to initiate data transmission.
% 4. Continuously reads incoming data, parses it, and stores it in a matrix, 
%    while also timestamping each reading.
% 5. Real-time plotting of the sensor data, updating the display at each iteration.
% 6. Handles any errors during execution by saving the valid data collected up to the 
%    point of the error in a .mat file, with timestamps for debugging and analysis.
% 7. Properly closes the serial connection upon completion.

% Key Variables:
% - port: COM port to connect to the device.
% - baudRate: Baud rate for communication.
% - maxPoints: Maximum number of data points to display in the real-time plot.
% - maxIterations: Maximum number of iterations to run the data collection loop.
% - dataMatrix: Matrix to store the sensor data (4 columns expected from the device).
% - timeStamps: Array to store timestamps corresponding to each data entry.
% - s: Serial object used for communication with the device.
% - hPlot: Handle for the real-time plot.

% Example Usage:87
% Update the 'port' variable to match the COM port of your device, and adjust 
% 'baudRate' as needed. DON'T forget change the beginning char 'a' as expected.
% Run the script to start collecting and plotting sensor data.



% Clear the workspace
clear;
clc;


%put 1.25 kg on load cellA(on the foot heel, 15cm moment arm ) when motor is on
% note_tosave = 'noload 20rpm';
note_tosave = ' plantar 15 to dorsi 10 - dynamic trial';
% note_tosave = '1.25kg loading test, 50rpm, 12.5cm from output axis to weight mass center towards toe, reduce jerk accel to 3000 rpm/s' ;
port = 'COM21';  % Update with your COM port
baudRate = 115200;

s = serialport(port, baudRate);

% to solve issue that PCB does not send data after several minmutes
flush(s);  % 清空缓冲区

% Configure the Bluetooth object to use a CR/LF (Carriage Return/Line Feed) terminator
% This is necessary because each line of data from the device ends with CR/LF
configureTerminator(s, "CR/LF");

% Set fixed y-axis limits
% yMin = -20;  % Adjust as needed
% yMax = 20;   % Adjust as needed


% Set a maximum number of points to display in the plot at one time
maxPoints = 200;

% Set a maximum number of iterations
maxIterations = 80*20*60;
iteration = 0;

% Initialize a matrix to store all received data and an array for timestamps (preallocate)
% Assuming 4 columns as per the data format: "45 322 545 65"
dataMatrix = NaN(maxIterations, 4);  % Initialize with NaNs
timeStamps = NaN(1, maxIterations);  % Initialize timestamps array


% Send the character 'c' to the Arduino via COM port
% zero the output
% write(s, 'c', "string");


% Start the timer to measure the overall runtime
tic


try

    % Create a plot handle with connecting lines
    hPlot = plot(0, 0, '.-', 'LineWidth', 2);  % Connect the data points with lines
    xlabel('Iteration');
    ylabel('Sensor Reading(Dedgree)');
    title('Connected Sensor Reading Over Time');
    grid on;
    set(gca, 'FontSize', 20);


    % Set y-axis limits before entering the loop (optional, adjust as needed)
    % ylim([yMin, yMax]);

    % Infinite loop to continuously read data from the Bluetooth device
    while (iteration < maxIterations)

        % Check if data is available
        if s.NumBytesAvailable  > 0

            % Read the line of data
            dataLine = readline(s);
            % disp(dataLine);  % Uncomment to display the raw data line (for debugging)
            
            % Split the line into individual numbers
            dataValues = strsplit(dataLine);
            % disp(dataValues);  % Uncomment to display the parsed data (for debugging)
            
            % Check if the data is valid and has exactly 4 elements
            if length(dataValues) == 5 && dataValues(1) == 'a'
                iteration = iteration + 1;
                dataVector = str2double(dataValues(2:end));
                dataMatrix(iteration, :) = dataVector;
                timeStamps(iteration) = iteration;
            else
                continue;
            end
            
            % Extract the third column (sensor data) from the data matrix
            sensorData = dataMatrix(:, 2);
            
            % Ensure that the lengths of timeStamps and sensorData do not exceed maxPoints
            if iteration >= maxPoints
                timeStamps_trunc = timeStamps(iteration-maxPoints+1:iteration);
                sensorData_trunc = sensorData(iteration-maxPoints+1:iteration);
            else
                timeStamps_trunc = timeStamps;
                sensorData_trunc = sensorData;  
            end
        
            % Update the plot with connected data points
            set(hPlot, 'XData', timeStamps_trunc, 'YData', sensorData_trunc);
            drawnow
        end
    end

    timestamps = datetime('now', 'Format', 'yyyy_MM_dd_HH_mm_ss');
    filename = sprintf('%s_loadcell_data.mat', timestamps);
    dataMatrix_valid = dataMatrix;
    timeStamps_valid = timeStamps;
    save(filename, 'dataMatrix_valid', 'timeStamps_valid', 'note_tosave');
    fprintf('Data saved to %s\n', filename);

catch exception
    % Display the error message
    disp(['Error: ' exception.message]);
    toc                     % Display the elapsed time up to the point of the error

    % Generate a timestamp string
    timestamps = datetime('now', 'Format', 'yyyy_MM_dd_HH_mm_ss');
    filename = sprintf('%s_loadcell_data.mat', timestamps);

    % Save the collected data before exiting
    validIndices = ~isnan(dataMatrix(:, 2));  % Get valid data only
    dataMatrix_valid = dataMatrix(validIndices, :);
    timeStamps_valid = timeStamps(validIndices);
    
    % Save the valid data to a .mat file
    save(filename, 'dataMatrix_valid', 'timeStamps_valid', 'note_tosave');
    fprintf('Data saved to %s\n', filename);

end

% Close the Bluetooth connection correctly
clear s;
fprintf('UART connection closed.\n');