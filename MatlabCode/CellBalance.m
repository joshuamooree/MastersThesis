%function to perform balancing
function [iBalance, pWaste]=CellBalance(method, capacity, v, iStack, soc)
	persistent balanceState;
	switch upper(method)
		case 'RESET',
			balanceState=zeros(length(v));
		case 'SOCPASSIVE'
			if max(size(balanceState))==0;
				balanceState=[];
			end
			%the state of charge method looks at
			%the state of charge and if any of the cells are more than
			%bancePointBegin percent away from the average, balancing begins
			%if they are less than balancePointEnd percent away from the average
			%balancing ends.  This provides some hysteresis.
			balancePointBegin=0.001;
			balancePointEnd=0.0005;
			
			iBalance=zeros(size(soc));

			cellsToBalance=SOCFindCells(balanceState, balancePointBegin, balancePointEnd, soc);
			balanceState=cellsToBalance;

			%discharge current is negative
			iBalance(cellsToBalance)=-0.1*mean(capacity);
			pWaste=-iBalance.*v;
		case 'SOCACTIVE'
			%similar logic to SOC passive except insead up just discharging, take the negative discharge
			%current and add it to the rest of the cells. Of course since this current is at a higher voltage
			%the cell voltage, to maintain constant power, the current must be (including efficiency losses),
			%                     V_cellDischarge
			%I_charge=I_discharge* ---------------*efficiency
			%                     V_cellsCharge
			balancePointBegin=0.01;
			balancePointEnd=0.005;
			efficiency=0.85;
			
			iBalance=zeros(size(soc));

			cellsToBalance=SOCFindCells(balanceState, balancePointBegin, balancePointEnd, soc);
			balanceState=cellsToBalance;

			%discharge current			
			iBalance(cellsToBalance)=-0.1*mean(capacity);
			
			%get the waste power before we add the cycled current back in
			pWaste=-iBalance.*v*(1-efficiency);

			%now the cycled back current
			iBalance+=ones(size(soc))*sum(iBalance.*v)/sum(v)*efficiency;
		case 'BIACTIVE'
			eff=0.85;
			N=max(size(soc));
			Q_p=zeros(1, length(N));
			[Q, indices]=sort(capacity);
			for k=1:N
				Q_p(k)=(eff*sum(Q(k+1:N))+sum(Q(1:k)))/...
				(eff*N-eff*k+k);
			end
			Q_p=min(Q_p);

			Q_n = capacity;
			
			iBalance=iStack.*((Q_n./Q_p-1)- ...
					   eff/(N*eff-N)*(sum(Q_n./Q_p)-N));
			
%			iBalance(find(iBalance > 0))=0;

			%to calculate waste, we start from the calculated balance current.  Since efficiency is
			%       P_out
			% eff = -----
			%       P_in
			%Then, Pwaste = P_in - P_out = cellV*abs(iBalance) - eff*cellV*abs(iBalance) = cellV*abs(iBalance)*(1-eff)
			pWaste=v.*iBalance*(eff-1);


			%next, we need to find the current that the balancer adds to each cell
			%since P_out = eff*P_in, by superposition,
			%P_stack = I_stack*sum(cellV) = eff * sum(iBalance)/(sum(cellV)/cellV_i) * sum(cellV),
			%so, I_stack = eff*sum(iBalance)/(sum(cellV)/cellV_i)
			iBalance += eff*sum(-iBalance)./N;

		case 'UNIACTIVE'
			E=0.85;
			N=max(size(soc));
			Q_p=zeros(1, N);
			[Q, indices]=sort(capacity);
			for k=1:N
				Q_p(k)=(E*sum(Q(k+1:N))+sum(Q(1:k)))/...
				(E*N-E*k+k);
			end
			Q_p=min(Q_p);

			Q_n = capacity;
			iBin=1;
			iBinPrev=0;

			maxErr=0.0001;
			while abs(iBin-iBinPrev) > maxErr
			        iBinPrev = iBin;
			        maxIBin=max(iBin(find(iBin>0)));
			        if isempty(maxIBin)
			                maxIBin=0;
			        end

			        iBin=(iStack-maxIBin*(1-E)).*((Q_n./Q_p-1)- ...
			                 E/(N*E-N)*(sum(Q_n./Q_p)-N));
			end

			iBalance = iBin;	
			iBalance -= max(iBin);

			%to calculate waste, we start from the calculated balance current.  Since efficiency is
			%       P_out
			% eff = -----
			%       P_in
			%Then, Pwaste = P_in - P_out = cellV*abs(iBalance) - eff*cellV*abs(iBalance) = cellV*abs(iBalance)*(1-eff)
			pWaste=v.*iBalance*(E-1);


			%next, we need to find the current that the balancer adds to each cell
			%since P_out = eff*P_in, by superposition,
			%P_stack = I_stack*sum(cellV) = eff * sum(iBalance)/(sum(cellV)/cellV_i) * sum(cellV),
			%so, I_stack = eff*sum(iBalance)/(sum(cellV)/cellV_i)
			iBalance += E*sum(-iBalance)./N;

		case 'UNIACTIVEFLAWED'
			E=0.85;
			N=max(size(soc));
			Q_p=zeros(1, N);
			[Q, indices]=sort(capacity);
			for k=1:N
				Q_p(k)=(E*sum(Q(k+1:N))+sum(Q(1:k)))/...
				(E*N-E*k+k);
			end
			Q_p=min(Q_p);

			Q_n = capacity;

			iBalance=iStack.*((Q_n./Q_p-1)- ...
					   E/(N*E-N)*(sum(Q_n./Q_p)-N));
	
			iBalance -= max(iBalance);

			%to calculate waste, we start from the calculated balance current.  Since efficiency is
			%       P_out
			% eff = -----
			%       P_in
			%Then, Pwaste = P_in - P_out = cellV*abs(iBalance) - eff*cellV*abs(iBalance) = cellV*abs(iBalance)*(1-eff)
			pWaste=v.*iBalance*(E-1);


			%next, we need to find the current that the balancer adds to each cell
			%since P_out = eff*P_in, by superposition,
			%P_stack = I_stack*sum(cellV) = eff * sum(iBalance)/(sum(cellV)/cellV_i) * sum(cellV),
			%so, I_stack = eff*sum(iBalance)/(sum(cellV)/cellV_i)
			iBalance += E*sum(-iBalance)./N;


		case {'NULL' 'NONE'}
			iBalance=zeros(size(v));
			pWaste=iBalance;
		otherwise
			error('Unknown method');
		
	end
end

function list=SOCFindCells(lastList, startPercent, stopPercent, soc)
	avgSOC=mean(soc);

	cellsToBalance=find(soc > avgSOC+startPercent);
	%add in the ones we were balancing before
	list=unique([lastList cellsToBalance]);

	cellsToStopBalancing=find(soc < avgSOC+stopPercent);
	list=setxor(list, cellsToStopBalancing);
end
