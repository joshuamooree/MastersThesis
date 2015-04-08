a_0=0.352;
a_1=0.068;

I_CH=25;
I_DCH=5;

minRi=1e2;
maxRi=1e9;

cycles=2000;
minRiSOC=ones(1, 2*cycles);
maxRiSOC=ones(1, 2*cycles);

for i=1:cycles
	if (i ~= 1)
		%charge
		minRiSOC(2*i-1)=(minRiSOC(2*(i-1))-I_CH*minRi/a_1+a_0/a_1)*...
			((maxRiSOC(2*(i-1))+a_0/a_1-I_CH*maxRi/a_1)/...
			(1+a_0/a_1-I_CH*maxRi/a_1))^(-maxRi/minRi)...
			-a_0/a_1+I_CH*minRi/a_1;

		maxRiSOC(2*i-1)=(maxRiSOC(2*(i-1))-I_CH*maxRi/a_1+a_0/a_1)*...
			((maxRiSOC(2*(i-1))+a_0/a_1-I_CH*maxRi/a_1)/...
			(1+a_0/a_1-I_CH*maxRi/a_1))^(-maxRi/maxRi)...
			-a_0/a_1+I_CH*maxRi/a_1;
	end

	%discharge
	minRiSOC(2*i)=(minRiSOC(2*i-1)+I_DCH*minRi/a_1+a_0/a_1)*...
		(minRiSOC(2*i-1)/(a_0/a_1+I_DCH*minRi/a_1)+1)^(-minRi/minRi)...
		-a_0/a_1-I_DCH*minRi/a_1;


	maxRiSOC(2*i)=(maxRiSOC(2*i-1)+I_DCH*maxRi/a_1+a_0/a_1)*...
		(minRiSOC(2*i-1)/(a_0/a_1+I_DCH*minRi/a_1)+1)^(-minRi/maxRi)...
		-a_0/a_1-I_DCH*maxRi/a_1;
end

%subplot(2, 2, 1)
%plot(1:cycles, minRiSOC(1:2:cycles*2))
%title('Charge SOC vs cycle for cell with min(R_i)')
%subplot(2, 2, 2)
%plot(1:cycles, minRiSOC(2:2:cycles*2))
%title('Discharge SOC vs cycle for cell with min(R_i)')
%subplot(2, 2, 3)
%plot(1:cycles, maxRiSOC(1:2:cycles*2))
%title('Charge SOC vs cycle for cell with max(R_i)')
%subplot(2, 2, 4)
%plot(1:cycles, maxRiSOC(2:2:cycles*2))
%title('Discarge SOC vs cycle for cell with max(R_i)')

colors=varycolor(4);
hold off;
plot(1:cycles, minRiSOC(1:2:cycles*2), 'color', colors(1, :))
hold on;
plot(1:cycles, maxRiSOC(1:2:cycles*2), 'color', colors(2, :))
plot(1:cycles, minRiSOC(2:2:cycles*2), 'color', colors(3, :));
plot(1:cycles, maxRiSOC(2:2:cycles*2), 'color', colors(4, :))

xlabel('Cycles')
ylabel('SOC');
h=legend('SOC after charge for min', 'SOC after charge for max', 'SOC after discharge for min', 'SOC after discharge for max')
set(h, 'location', 'northeast');

