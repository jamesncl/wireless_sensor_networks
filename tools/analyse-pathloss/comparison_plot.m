
% Note: use in conjenction with the modified connectivityMap simulation
% This outputs data points of distance vs RSSI, which are saved into data.txt

M = csvread("data.txt");
distances = M(:,1);
observed_rssi = M(:,2);

pathLossExponent = 2.4;
PLd0 = 55;
d0 = 1.0;
sigma = 4.0;
transmissionPower = 0;

pathLossFunc = @(distance) PLd0 + (10 * pathLossExponent * log10(distance/d0)) + (randn()*sigma);

plot(distances, observed_rssi, 'LineWidth',2)

hold on;
for i = 1:10
    expectedPathLosses = transmissionPower - arrayfun(pathLossFunc, distances);
    plot(distances, expectedPathLosses, 'o')
end
hold off;
