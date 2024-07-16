% Define parameters
clc,clear
numBytes = 12;  % Length of random strings
numStrings = 1000;  % Number of random strings to generate
outputFileName = sprintf('tx_payload_%dx%dB.txt',numStrings,numBytes);
characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';

fileID = fopen(outputFileName, 'w');
% Generate random strings
for i = 1:numStrings
    randomIndex = randi(length(characters), 1, numBytes);
    randomString = characters(randomIndex);
    fprintf(fileID, '%s,', randomString);
end


