function returnVal=plotLabData(file, logMode, nCells)
global thesisMode
%file='algo21.csv';

if nargin == 2
	nCells=12;
end

data=dlmread(file, '\t');

%logMode = '1';
%logMode = '29';
logMode = hex2dec(logMode);

logRaw = 1;
logOCV = 2;
logBalancer = 3;
logStackI = 4;
logR1I = 5;
logPWMThresh = 6;
logNone = 7;

offset=2; %all logs have timestamps
spacing=1; %all logs have at least the current accumulators 

if (bitget(logMode, logBalancer))
  offset += 1;
end

if (bitget(logMode, logStackI))
  offset += 1;
end

if (bitget(logMode, logRaw))
  spacing += 1;
end

if (bitget(logMode, logOCV))
  spacing += 1;
end

if (bitget(logMode, logR1I))
  spacing += 1;
end

if (bitget(logMode, logPWMThresh))
  spacing += 1;
end


printf 'Done Loading data'
data=sortrows(data, 1);
data=data(find(data(:,1)~=0),:);
[row, col]=find(sum(data(:,(0:nCells-1)*spacing+offset)<10, 2)==nCells);
data=data(row,:);

[row, col]=find(sum(data(:,(0:nCells-1)*spacing+offset)>2, 2)==nCells);
data=data(row,:);
data=data(2:end,:);
%at this point, we have the data and we've removed any garbage
printf 'Done Parsing Data'






%so, first voltage vs time
time=data(:,1);

returnVal.time=time;
returnVal.voltage=data(:, spacing*(0:(nCells-1))+offset);
returnVal.currentCount=data(:,spacing*(0:(nCells-1))+1+offset);

if thesisMode
	t2 = min(time):(max(time)-min(time))/2000:max(time);
end

colors=varycolor(nCells);

clf;
close all;

hold on;
legendStrings=[];
for i=1:nCells
%    if i == 1 || i == 8
	if thesisMode
		plot(t2, interp1(time, data(:, spacing*(i-1)+offset), t2), 'color', colors(i, :));
	else
		plot(time, data(:, spacing*(i-1)+offset), 'color', colors(i, :));
	end
	legendStrings=[legendStrings; 'Cell ', num2str(i)];
%     end
end
legend(legendStrings, 'location', 'southeast');
xlabel('Time (s)');
ylabel('Voltage (V)');

return;

%then amp-seconds vs time
subplot(3, 1, 2)
hold on;
for i=1:nCells
	plot(time, data(:,spacing*(i-1)+1+offset), 'color', colors(i,:));
end
xlabel('Time (s)');
ylabel('Charge (A-s)');

%then amp-hours vs voltage
subplot(3,1,3)
hold on;
for i=1:nCells
	plot(data(:, spacing*(i-1)+offset), data(:,spacing*(i-1)+1+offset), 'color', colors(i,:));
end
xlabel('Voltage (V)');
ylabel('Charge (A-s)');
%legend(legendStrings);


