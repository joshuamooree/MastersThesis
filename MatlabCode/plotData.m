%plotData
%arguments:
% type: one of 2DSOCOCV, 2DSOCOCVI, 3DSOC
%arguments for 2DSCOOCV (2d SOC and OCV plot for one cell over time):
%	1) time vector
%	2) cell voltage
%	3) SOC matrix
%	4) OCV matrix
%	5) cell index (optional - if missing plot all cells)
%	time should be the rows of cell voltage, SOC, and OCV and cell voltage, OCV, and SOC should be the same size
%arguments for 2DSOCOCVI
%	same as 2DSOCOCV except
%	5) current matrix
%	6) cell index (optional, if missing plot all cells)
%arguments for 3DSOC
%	1) time vector
%	2) SOC matrix (dim 1=time, dim 2=macroCell dim 3=cells
%	3) time index (optional, if not supplied, will plot the average of each macro cell
%	   for the entire array versus time, with color representing deviation from average.
%          If supplied, will plot individual cells at that time step.
%	SOC matrix must be 3 dimensional
function plotData(type, varargin)
	global thesisMode

	switch upper(type)
	case {'2DSOCOCV', '2DSOCOCVI'}
		if strcmp(upper(type),'2DSOCOCV')
			if nargin~= 5 && nargin~=6
				error 'incorrect number of arguments'
			end
			nplots=3;
		else
			if nargin ~=6 && nargin ~=7
				error 'incorrect number of arguments'
			end
			nplots=4;
		end
		t=varargin{1};
		v=varargin{2};
		SOC=varargin{3};
		OCV=varargin{4};

		%drop the last point (needed because simulator initializes entire array and cannot fill entire array)
		t=t(1:length(t)-1);
		v=v(1:length(v)-1, :, :);
		SOC=SOC(1:length(SOC)-1, :, :);
		OCV=OCV(1:length(OCV)-1, :, :);

		if length(t) ~= size(v)(1)
			error 'Cell voltage number of rows must match number of columns in time'
		elseif length(t) ~= size(SOC)(1)
			error 'SOC number of rows must match number of columns in time'
		elseif length(t) ~= size(OCV)(1)
			error 'OCV number of rwos must match number of columns in time'
		elseif size(v) ~= size(SOC) || size(SOC) ~= size(OCV)
			error 'cell voltage, SOC, and OCV must be the same same size'
		end

		nMC=size(v)(2);
		if ndims(v)==3
			nCellsPerMC=size(v)(3);
		else
			nCellsPerMC=1;
		end

		if strcmp(upper(type), '2DSOCOCVI')
			I=varargin{5};
	
			I=I(1:length(I)-1, :, :)
			if length(t) != size(I)(1);
				error 'Current matrix number of rows must match number of columns in time'
			elseif size(OCV)!= size(I)
				error 'current vector, cell voltage, SOC and OCV must be the same size'
			end
			
			%handle the cases where only one cell is plotted or all the cells are plotted
			if nargin==7
				plot=varargin{6};
				%fixme - make multidim
				if plot > nMC
					error 'Cell index must be within, V, SOC, OCV, I matrices'
				end
				colors='k';
			else
				plots=1:nMC*nCellsPerMC;
				colors=varycolor(nMC*nCellsPerMC);
			end
		else
			%handle the cases where only one cell is plotted or all the cells are plotted
			if nargin==6
				plots=varargin{5};
				%fixme - make multidim
				if plots > nMC
					error 'Cell index must be within V, SOC, OCV matrices'
				end
				colors='k';
			else
				plots=1:nMC*nCellsPerMC;
				colors=varycolor(nMC*nCellsPerMC);
			end
		
		end
		%now that we have figured out all the arguments and gotten everything in the right format, finally plot the stuff
		%todo - make multidim

		clf;
		if ~thesisMode || isempty(thesisMode)
			subplot(nplots, 1, 1);
		end

		if thesisMode
			t2 = min(t):(max(t)-min(t))/2000:max(t);
		end

		hold on;
		for plotI=plots
			if thesisMode
				v2 = interp1(t, v(:,plotI)', t2);
				plot(t2, v2, 'color', colors(plotI, :));
			else
				plot(t, v(:,plotI)', 'color', colors(plotI, :));
			end
		end
		xlim([0, max(t)]);
		if length(plots)==1
			ylim([min(v(:,plots)) max(v(:,plots))]);
		else
			ylim([min(min(v)) max(max(v))]);
		end
		ylabel('Cell voltage (V)');
		hold off;

		if thesisMode
			thesisFormat
			figure;
		else 
			subplot(nplots, 1, 2);
		end
	
		hold on;
		for plotI=plots
			if thesisMode
				SOC2 = interp1(t, SOC(:,plotI)', t2);
				plot(t2, SOC2, 'color', colors(plotI, :));
			else
				plot(t, SOC(:,plotI)', 'color', colors(plotI, :));
			end
		end
		xlim([0, max(t)]);
		ylabel('SOC');
		hold off;

		if thesisMode
			thesisFormat
			figure;
		else
			subplot(nplots, 1, 3);
		end

		hold on;
		for plotI=plots
			if thesisMode
				OCV2 = interp1(t, OCV(:,plotI)', t2);
				plot(t2, OCV2, 'color', colors(plotI, :));	
			else 
				plot(t, OCV(:,plotI)', 'color', colors(plotI, :));
			end
		end

		xlim([0, max(t)]);
		ylabel('OCV (V)');
		hold off;

		if thesisMode
			thesisFormat
		end


		if nplots == 4
			if thesisMode
				figure;
			else
				subplot(nplots, 1, 4);
			end

			xlim([0, max(t)]);
			if thesisMode
				I2 = interp1(t, I(:,Cell)', t2);
				plot(t2, I2, 'color', colors(Cell, :));
			else
				plot(t, I(:,Cell)', 'color', colors(Cell, :));
			end
			xlim([0, max(t)]);
			ylabel('I');
			hold off;

			if thesisMode
				thesisFormat
			end
		end
		xlabel('Time (s)');
	case '3DSOC'
		if nargin == 4
			t=varargin{1};
			SOC=varargin{2};
			n=varargin{3};
			
			%drop the last point (usually weird)
			SOC=SOC(1:length(SOC)-1, :, :);
			t=t(1:length(t)-1);

			clf;
			surf(1:(size(SOC)(2)), 1:(size(SOC)(3)), shiftdim(SOC(n,:,:), 1)', 'EdgeColor', 'none', 'LineStyle', 'none');
			
		elseif nargin == 3
			t=varargin{1};
			SOC=varargin{2};
			%drop the last point (usually weird)
			SOC=SOC(1:length(SOC)-1, :, :);
			t=t(1:length(t)-1);
	
			SOCMC=mean(SOC, 3);
			%maxDelta=max(SOC-repmat(SOCMC, [1 1 size(SOC)(3)]), [], 3)-min(SOC-repmat(SOCMC, [1 1 size(SOC)(3)]), [], 3);
			maxDelta=max(SOC, [], 3)-min(SOC, [], 3);
			maxDelta./=SOCMC;

			printf('PlotData: Maximum variation across macrocells is %G\n', max(max(maxDelta(4000, :, :), [], 1)));
			
			clf;
			surf(1:(size(SOC)(2)), t, SOCMC, maxDelta, 'EdgeColor', 'none', 'LineStyle', 'none');
			colormap(jet)
		end
		
	otherwise
		error 'Invalid type of plot'
end
