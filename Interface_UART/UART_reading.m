% Clear the workspace
clear;
clc;
% Open a serial port
port = 'COM13';  % Update with your COM port
baudRate = 115200;

s = serialport(port, baudRate);
configureTerminator(s, "LF");  % Assuming line feed is used as terminator

% Set fixed y-axis limits
yMin = -180;
yMax = 180;

% Initialize variables for storing data
timeStamps = [];
sensorData = [];

maxPoints = 100;

try
    % Set a maximum number of iterations
    maxIterations = 200;
    iteration = 0;

    % Create a plot handle with connecting lines
    hPlot = plot(0, 0, '*-', 'LineWidth', 1);  % Connect the data points with lines
    xlabel('Iteration');
    ylabel('Sensor Reading');
    title('Connected Sensor Reading Over Time');
    grid on;

    % Set y-axis limits before entering the loop
    ylim([yMin, yMax]);

    while (1)
        iteration = iteration + 1;

        % Read a line from the serial port
        data = readline(s);

        % Convert the received data to a numeric value
        sensorReading = str2double(data);

        % Display received data for debugging
        %fprintf('Received: %s\n', data);

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

        % Pause to allow time for plotting (adjust as needed)
        % pause(0.1);
        drawnow
    end

catch ex
    % Display the error message
    disp(ex.message);
end

% Close the serial port when done
fclose(s);
delete(s);
fprintf('Serial port closed.\n');
