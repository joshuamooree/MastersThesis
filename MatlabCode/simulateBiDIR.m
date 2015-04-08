model1=load('Cell Models/ENERGYmodel.mat');
model=model1.model;
data=load('Profiles/udds-normalized.txt');
%data=load('Profiles/UDDS_Simulation_LFP_Energy.txt');
%data=load('Profiles/Phil_042510_1355_Dynamic_LFP_Energy_25C.csv');

%this model uses rows for time steps, columns for macro cells, and height for individual cells
nMacroCells=3;
cellsPerMC=1;
capacityMod=[1, 1, 2];

time=data(:,1);
i_orig=data(:, 2);

iChg=5;
ncycles=10;
dischargeMult=10;
chargeT=500;
[i, t, chgIdx, dchgIdx]=makeLoadTimeVector(nMacroCells, ncycles, iChg, chargeT, -5, 1000);
%[i, t, chgIdx, dchgIdx]=makeLoadTimeVector(nMacroCells, ncycles, iChg, chargeT, i_orig*cellsPerMC*dischargeMult, time);

%if the data file has a third column, this is data from an test run.  Extract that column so we can plot real cell voltage
%versus simulated cell voltage later.
if size(data, 2)==3
	actualV=data(:,3);
	actualV=interp1(time, actualV, t, 'linear');
end

selfDischargeRate=1; %percent per week

%I_monitoring=1e-3;
I_monitoring=0;
I_monitoring.*=ones(1, nMacroCells);

%colbEff=0.98;
colbEff=1;
colbEffStdDev=0.0;
eta=colbEff+colbEffStdDev*randn([1, nMacroCells, cellsPerMC]);

meanCapacity=1;
capacityMult=meanCapacity+0.0*randn([1, nMacroCells, cellsPerMC]);

RVariationPercentStdDev=1e4;
selfDischargeRVariation=RVariationPercentStdDev.*randn([nMacroCells, cellsPerMC])';
selfDischargeRVariation+=abs(min(selfDischargeRVariation))+1;

temp=25;

beta=getParam('BParam', temp, model);
c_1=getParam('CParam', temp, model);
Q=getParam('CnParam', temp, model);
Q=Q.*capacityMod;
R=getParam('RParam', temp, model)*ones(1, nMacroCells, cellsPerMC);
M=getParam('MParam', temp, model);
%gamma=getParam('GParam', temp, model);
gamma=0.1;

alpha_2=0.99999;
alpha_1=0.99998*tanh(beta);
c_2=-c_1*(1-alpha_2)/(1-alpha_1);

%need to go from capacity and self discharge rate to an R
%capacity is amp-hours
%self discharge rate is percent per week
%since R should be V/I
%I = capacity*self discharge/1% = amp-hour*(%/week)/%=amp/24/7
%so, R is the voltage / the current found above
%for now, assume the V is the OCV at 50% charge
%selfDischargeR=OCVfromSOCtemp(0.5, temp, model)/(Q*selfDischargeRate/(100*24*7))*selfDischargeRVariation;
selfDischargeR=[1e12, 1e12, 1e12];
I_selfDischarge=zeros(nMacroCells, cellsPerMC);

%allocate and initialize memory for results
y=ones(length(t), nMacroCells, cellsPerMC).*OCVfromSOCtemp(1, temp, model);
z=ones(length(t), nMacroCells, cellsPerMC);
OCV=ones(length(t), nMacroCells, cellsPerMC).*OCVfromSOCtemp(1, temp, model);
h=zeros(length(t), nMacroCells, cellsPerMC);
pDissipation=zeros(length(t), nMacroCells);
i_balance=zeros(length(t), nMacroCells);
I_cell=zeros(length(t), nMacroCells, cellsPerMC);

f_1=zeros(1, nMacroCells, cellsPerMC);
f_2=zeros(1, nMacroCells, cellsPerMC);
h=zeros(1, nMacroCells, cellsPerMC);

%stuff to model cell to cell interactions within a macrocell
%the equations look like
%I_R1 = (V_MC-V_1)/R_1
%I_R2 = (V_MC-V_2)/R_2
%...
% and
% I_R1 + I_R2 + ... = I_MC
%so, we need to put this all in a matrix and solve
I_rL=diag(ones(cellsPerMC*nMacroCells, 1));
I_MCL=mat2cell(repmat(ones(1, cellsPerMC), nMacroCells, 1), ones(nMacroCells, 1), cellsPerMC);
I_MCL=blkdiag(I_MCL{:});
left=[I_rL; I_MCL];
%since the right side changes with each loop, that will be done in the simulation loop


%we assume (initially) that the current splits evenly
I_cell(1,:,:)=repmat(i(1,:), cellsPerMC, 1)'.*Q'/cellsPerMC;
for n=[1:(length(t)-2)]

        %if one of the cells has reached maximum charge, stop charging
	if (max(max(z(n,:,:))) >= 1) && (mean(i(n,:)) > 0)
		i(n,:)=zeros(size(i(n,:)));
	end
	%if one of the cells has reached minimum charge, stop discharging
	if (min(min(z(n,:,:))) <= 0) && (mean(i(n,:)) < 0)
		i(n,:)=zeros(size(i(n,:)));
	end

	%get the array of I values for this time
	%and add the various terms in:
	%self discharge current, monitoring current, balance current
	%then, if the cell is charging, reduce the charge current by eta
	I_nMC=i(n,:)*Q'-mean(I_selfDischarge,3)'-I_monitoring;
	[i_balance(n,:), pDissipation(n,:)]=CellBalance('BiActive', Q, mean(y(n,:,:),3), I_nMC, mean(z(n,:,:),3));
	I_nMC+=i_balance(n,:);
	chargePts=find(I_nMC>0);
	I_nMC(chargePts).*=eta(chargePts);
	
	%stuff to model cell to cell interactions within a macrocell
	%right side of the equation and solution
	%the macro cell voltage is V_MC=V_x+I_R*R, for any cell - use the first cell here
	V_MC=OCV(n,:,1)+I_cell(n,:,1).*mean(R, 3);

	%first, we need to take V_MC (row vector) and duplicate it so we have voltage for each cell.
	%We should get an array that is cellsPerMC x nMacroCells.
	%next, we take OCV for this time step (will be 1 x nMacroCells x cellsPerMC) and reduce dimensions and reshape
	%and subtract it from V_MC
	%the divide by R (1 x nMacroCells x cellsPerMC)
	%then reshape to be a column vector nMacroCells*cellsPerMC x 1
	I_rR=reshape((repmat(V_MC, cellsPerMC, 1)-shiftdim(OCV(n,:,:), 1)')./shiftdim(R, 1)', nMacroCells*cellsPerMC, 1);
	right=[I_rR; I_nMC'];
	
	I_n=shiftdim(reshape(left\right, cellsPerMC, nMacroCells)', -1);
	I_cell(n+1,:,:)=I_n;
	
	%i_n=repmat(i_nMC, [1 1 cellsPerMC]);

	%now for the enhanced self correcting model
	dt=(t(n+1)-t(n))/3600;	%convert to hours since battery capacity is in amp hours
	z(n+1,:, :)=z(n,:, :)+I_n*dt./Q;
	f_1=alpha_1*f_1-1e-4*I_n;
	f_2=alpha_2*f_2-1e-4*I_n;
	h(n+1,:, :)=exp(-abs(gamma*I_n*dt./Q)).*h(n,:, :)+(1-exp(-abs(gamma*I_n*dt./Q)))*M.*-sign(I_n);
	y(n+1, :, :)=OCVfromSOCtemp(z(n, :, :), temp, model)+I_n.*R+c_1*f_1+c_2*f_2+M*h(n+1, :, :);
	OCV(n, :, :)=OCVfromSOCtemp(z(n, :, :), temp, model);

	%calculate self discharge current for next time	
	I_selfDischarge=squeeze(y(n,:, :)./selfDischargeR)';
end

close all
plotData('2DSOCOCV', t, mean(y, 3), mean(z, 3), mean(OCV, 3))
print -dpng 'ColbEffSim2.png'
if size(data, 2) == 3
	figure();
	plot(t, y(:,1)'-actualV);
	xlim([0, max(t)]);
	ylim([-0.1, 0.1]);
	xlabel('Time');
	ylabel('Simulated V - Actual V');
end

%figure();
%subplot(3, 1, 1)
%plot(t, i_balance(:,1))
%subplot(3, 1, 2)
%plot(t, i_balance(:,2))
%subplot(3, 1, 3)
%plot(t, i_balance(:,3))

