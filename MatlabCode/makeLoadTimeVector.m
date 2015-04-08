% Create a (possibly two dimensional) vector that has charge current.  Also, creates a new time
% vector.
% nCells: number of cells in the pack
% nCycles: number of charge/discharge cycles
% chargeI: current to charge cells at (constant current for now)
% chargeT: time to charge at chargeI
% dischargeI: can be either a scalar for a constant discharge current or a vector for a discharge profile
% dischargeT: discharge time (in seconds).  If dischargeT is a scalar, this must be a scalar.  If dischargeT is a vector,
%             this must also be vector of the same size as dischargeI (each point must correspond to the
%             corresponding point in dischargeI
function [i, t, chgIdx, dchgIdx]=makeLoadTimeVector(nCells, nCycles, chargeI, chargeT, dischargeI, dischargeT)
	if isscalar(dischargeI)
		if ~isscalar(dischargeT)
			error 'If dischargeI is a scalar, dischargeT must be as well';
		end

		%create a discharge vector with constant current
		dischargeI=ones(round(dischargeT), nCells)*dischargeI;
	else
		if isscalar(dischargeT)
			error 'If dischargeI is a vector, dischargeT must be as well';
		elseif size(dischargeI) ~= size(dischargeT)
			error 'dischargeI must be the same size as dischargeT'
		end
	
		%resample the discharge vector to be 1 second intervals	
		newTime=1:1:max(dischargeT);
		dischargeI=interp1(dischargeT, dischargeI, newTime, 'linear');
		dischargeI(find(isna(dischargeI)))=0;
		dischargeI=repmat(dischargeI, nCells, 1)';

		%don't need the whole discharge array anymore
		dischargeT=max(dischargeT);
	end
	
	%create the charge vector
	chargeI=ones(round(chargeT), nCells)*chargeI;
	%create the index vectors
	dchgIdx = [zeros(1,length(dischargeI)), 1, zeros(1, length(chargeI)-1)];
	chgIdx = [1, zeros(1, length(dischargeI)-1), zeros(1, length(chargeI))];
	chgIdx=find(repmat(chgIdx, 1, nCycles)==1);
	dchgIdx=find(repmat(dchgIdx, 1, nCycles)==1);

	%concatenate the discharge and charge vectors to make 1 cycle
	i=vertcat(dischargeI, chargeI);
	%now repeat that to make the specified number of cycles
	i=repmat(i, nCycles, 1);

	%make a new t vector
	t=1:1:(round(dischargeT)+round(chargeT))*nCycles;

end
