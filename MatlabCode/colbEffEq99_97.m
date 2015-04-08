%coullombic efficiency mismatch equations
minE=97;
maxE=99;
R=minE/maxE;

ncycles = 150;

SOCChargeMin=minE/maxE*(cumsum([R.^(0:(ncycles-1))]))-cumsum([0 R.^(1:(ncycles-1))]);
SOCChargeMax=maxE/maxE*(cumsum([R.^(0:(ncycles-1))]))-cumsum([0 R.^(1:(ncycles-1))]);

SOCDischargeMin=minE/maxE*(cumsum([R.^(0:(ncycles-1))]))-cumsum([R.^(1:(ncycles))]);
SOCDischargeMax=maxE/maxE*(cumsum([R.^(0:(ncycles-1))]))-cumsum([R.^(1:(ncycles))]);

colors=varycolor(4);
hold off;
plot(1:(ncycles+1), [1 SOCChargeMin], 'color', colors(1, :));
hold on;
plot(1:(ncycles+1), [1 SOCChargeMax], 'color', colors(2, :));
plot(1:(ncycles+1), [0 SOCDischargeMin], 'color', colors(3, :));
plot(1:(ncycles+1), [0 SOCDischargeMax], 'color', colors(4, :));

xlabel('Cycles')
ylabel('SOC');
h=legend('SOC after charge for min', 'SOC after charge for max', 'SOC after discharge for min', 'SOC after discharge for max')
set(h, 'location', 'east');
