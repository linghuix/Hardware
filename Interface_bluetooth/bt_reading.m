% Clear the workspace
clear;
clc;

% note_tosave = 'put 1.25 kg on load cellB(on the foot tip, 15cm moment arm ) when motor is on';

note_tosave = 'test';

%"first put 1.25 kg on load cell B (on the foot tip, 15cm moment arm ), " + ...
    %"then put 2.5 kg on load cell A (on the foot heel, 15cm moment arm)"

% Create a Bluetooth object to connect to the specified device
bt = bluetooth('KTHdynamometer2', 1);  % Adjust the device name ('KTHdynamomoter') and channel if needed

% to solve issue that PCB does not send data after several minmutes
flush(bt);  % 清空缓冲区

% Configure the Bluetooth object to use a CR/LF (Carriage Return/Line Feed) terminator
% This is necessary because each line of data from the device ends with CR/LF
configureTerminator(bt, "CR/LF");

% Set fixed y-axis limits
% yMin = -20;  % Adjust as needed
% yMax = 20;   % Adjust as needed


% Set a maximum number of points to display in the plot at one time
maxPoints = 100;

% Set a maximum number of iterations
maxIterations = 80*10*60;
iteration = 0;

% Initialize a matrix to store all received data and an array for timestamps (preallocate)
% Assuming 4 columns as per the data format: "45 322 545 65"
dataMatrix = NaN(maxIterations, 4);  % Initialize with NaNs
timeStamps = NaN(1, maxIterations);  % Initialize timestamps array


% Send the character 'c' to the Arduino via Bluetooth
write(bt, 'c', "string");


% Start the timer to measure the overall runtime
tic


try

    % Create a plot handle with connecting lines
    hPlot = plot(0, 0, '.-', 'LineWidth', 1);  % Connect the data points with lines
    xlabel('Iteration');
    ylabel('Sensor Reading(Dedgree)');
    title('Connected Sensor Reading Over Time');
    grid on;


    % Set y-axis limits before entering the loop (optional, adjust as needed)
    % ylim([yMin, yMax]);

    % Infinite loop to continuously read data from the Bluetooth device
    while (iteration < maxIterations)

        % Check if data is available
        if bt.NumBytesAvailable  > 0

            % Read the line of data
            dataLine = readline(bt);
            % disp(dataLine);  % Uncomment to display the raw data line (for debugging)
            
            % Split the line into individual numbers
            dataValues = strsplit(dataLine);
            % disp(dataValues);  % Uncomment to display the parsed data (for debugging)
            
            % Check if the data is valid and has exactly 4 elements
            if length(dataValues) == 5 && dataValues(1) == 'b'
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

    
catch exception
    % Display the error message
    disp(['Error: ' exception.message]);
    toc                     % Display the elapsed time up to the point of the error

    % Generate a timestamp string
    timestamps = datetime('now', 'Format', 'yyyy_MM_dd_HH_mm_ss');
    filename = sprintf('%s_loadcell_data.mat', timestamps);

    % Save the collected data before exiting
    validIndices = ~isnan(dataMatrix(:, 1));  % Get valid data only
    dataMatrix_valid = dataMatrix(validIndices, :);
    timeStamps_valid = timeStamps(validIndices);
    
    % Save the valid data to a .mat file
    save(filename, 'dataMatrix_valid', 'timeStamps_valid', 'note_tosave');
    fprintf('Data saved to %s\n', filename);

end

% Close the Bluetooth connection correctly
clear bt;
fprintf('Bluetooth connection closed.\n');