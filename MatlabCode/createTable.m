file='capacity_latest.csv';

nCells=12;
dataOrig=dlmread(file, '\t');

data = dataOrig;

data=sortrows(data, 1);
data=data(find(data(:,1)~=0),:);
[row, col]=find(sum(data(:,[2,4,6,8,10,12,14,16,18,20,22,24])<10, 2)==nCells);
data=data(row,:);

[row, col]=find(sum(data(:,[2,4,6,8,10,12,14,16,18,20,22,24])>2, 2)==nCells);
data=data(row,:);
data=data(2:end,:);
%at this point, we have the data and we've removed any garbage

%get the lowest cell (discharges first)
[x, ix]=min(min(data(:,(1:nCells)*2)));

cell=data(:,ix*2);

nPoints=32;
rescaleMin=3.0;
rescaleMax=4.2;
voltsPerTick=1.5e-3;
time=data(:,1);
cell=(cell-min(cell)).*(rescaleMax - min(cell))/(max(cell)-min(cell))+rescaleMin;
newTimes=linspace(min(time), max(time), nPoints);
interpCell=interp1(time, cell, newTimes);

figure;
subplot(2, 1, 1);
plot(time, cell);
subplot(2, 1, 2);
plot(newTimes, interpCell);

sort(round(interpCell/voltsPerTick))
