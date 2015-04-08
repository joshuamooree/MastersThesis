global thesisMode

desI=[1.2*ones(1,1000), 1.0*ones(1,500), -2*ones(1,100), -0.5*ones(1,500)];

limitF=inline('min(1.1*ones(size(I)),max(-1.5*ones(size(I)),I))');
errorSum=zeros(1,length(desI)+1);
outI=zeros(size(desI));

desiredCoulombCount=zeros(1,length(desI)+1);
actualCoulombCount=zeros(1,length(desI)+1);

for t=1:length(desI)
	outI(t)=limitF(desI(t)+errorSum(t));
	errorSum(t+1) = desI(t)-outI(t) + errorSum(t);
	desiredCoulombCount(t+1) = desI(t)+desiredCoulombCount(t);
	actualCoulombCount(t+1) = outI(t)+actualCoulombCount(t);
end

clf
if ~thesisMode || isempty(thesisMode)
	subplot(4,1,1)
end
hold on
plot(1:length(desI), desI)
plot(1:length(desI), outI, 'color', 'r')
title('Desired load and actual load')
legend('Desired load', 'Actual Load')
ylabel('Load (A)')
xlabel('Time (s)')
if thesisMode
	thesisFormat;
	figure;
else
	subplot(4,1,2)
end
plot(1:length(desI),errorSum(1:length(desI)))
title('Integrated Error')
ylabel('Error (A s)')
xlabel('Time (s)')
if thesisMode
	thesisFormat
	figure;
else
	subplot(4,1,3)
end
hold on
plot(1:length(desI),desiredCoulombCount(1:length(desI)))
plot(1:length(desI),actualCoulombCount(1:length(desI)), 'color', 'r')
title('Desired and actual output coulomb count')
ylabel('Coulomb Count (A s)')
xlabel('Time (s)')
legend ('Desired Coulomb Count', 'Actual Coulomb Count')
if thesisMode
	thesisFormat
	figure;
else
	subplot(4,1,4)
end
plot(1:length(desI),(desiredCoulombCount-actualCoulombCount)(1:length(desI)))
title('Instantaneous coulomb count error')
ylabel('Coulomb Count Error (A s)')
xlabel('Time (s)')
if thesisMode
	thesisFormat
end
