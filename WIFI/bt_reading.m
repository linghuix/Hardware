% Clear the workspace
clear;
clc;

% Create a Bluetooth object
bt = bluetooth('ESP32test_right', 1);  % Adjust device name if needed

% Set fixed y-axis limits
yMin = -180;  % Adjust as needed
yMax = 180;   % Adjust as needed

% Initialize variables for storing data
timeStamps = [];
sensorData = [];

maxPoints = 500;

try
    % Set a maximum number of iterations
    maxIterations = 200;
    iteration = 0;

    % Create a plot handle with connecting lines
    hPlot = plot(0, 0, '.-', 'LineWidth', 1);  % Connect the data points with lines
    xlabel('Iteration');
    ylabel('Sensor Reading(Dedgree)');
    title('Connected Sensor Reading Over Time');
    grid on;

    % Set y-axis limits before entering the loop
    ylim([yMin, yMax]);

    while (1)
        % Check if data is available
        if bt.BytesAvailable > 0
            % Read data from the Bluetooth connection
            data = fscanf(bt, '%s');
            iteration = iteration + 1;
            
            % Parse the received data (assuming it is in the format "value")

            sensorReading = str2double(data);

            % Display received data for debugging
            %fprintf('Received: %s\n', data);
            %fprintf('Received: %f\n', sensorReading);

            % Store the data for later
            timeStamps = [timeStamps, iteration];
            sensorData = [sensorData, sensorReading];
    
            % Ensure that the lengths of timeStamps and sensorData do not exceed maxPoints
            if length(timeStamps) > maxPoints
                timeStamps_trunc = timeStamps(end-maxPoints+1:end);
                sensorData_trunc = sensorData(end-maxPoints+1:end);
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
end

% Close the Bluetooth connection correctly
clear bt;
fprintf('Bluetooth connection closed.\n');