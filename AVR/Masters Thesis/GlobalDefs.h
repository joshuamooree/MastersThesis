#ifndef _GLOBALDEFS_H_
#define _GLOBALDEFS_H_

#define NumberOfCells 12
#define Dash2

#define cellOV 4.2
#define cellUV 3.0
#define minChargeI ((int16_t)(0.08*CurrentTick))	//0.04A
#define alarmOV ((int16_t)((cellOV*1.1)/LT6802CountValue))
#define alarmUV ((int16_t)((cellUV*0.9)/LT6802CountValue))
//#define alarmUV ((int16_t)((cellUV*0.5)/LT6802CountValue))

#define MaskedCells 0x0000

//call constants for the current sense
#define IMeasCalPosNum 727
#define IMeasCalPosDem 1000
#define IMeasCalNegNum 0
#define IMeasCalNegDem 1

#define BalancerCurrent	0.486
#define CurrentTick 163			    //e.g. 2^10=2.56A	or 1A=1V
									//so in ticks, the balancer current is 
									//BalancerCurrent*CurrentTick
								//163=0.92charge, reads 0.092
								//163=0.107 charge, reads 0.1472	
#define Bal1I 0.752
#define Bal2I 0.514
#define Bal3I 0.768
#define Bal4I 0.829
#define Bal5I 0.774
#define Bal6I 0.750
#define Bal7I 0.792
#define Bal8I 0.517
#define Bal9I 0.486
#define Bal10I 0.745
#define Bal11I 0.506
#define Bal12I 0.740

#define min(a, b) ((a) < (b)?(a) : (b))
#define min12(a, b, c, d,  e, f, g, h,  i, j, k, l) (min(min(min(min(min(min(min(min(min(min(min((a), (b)),(c)),(d)),(e)),(f)),(g)),(h)),(i)),(j)),(k)),(l)))
#define minBalCurrent min12(Bal1I, Bal2I, Bal3I, Bal4I, Bal5I, Bal6I, Bal7I, Bal8I, Bal9I, Bal10I, Bal11I, Bal12I)

#define max(a, b) ((a) > (b)?(a) : (b))
#define max12(a, b, c, d,  e, f, g, h,  i, j, k, l) (max(max(max(max(max(max(max(max(max(max(max((a), (b)),(c)),(d)),(e)),(f)),(g)),(h)),(i)),(j)),(k)),(l)))
#define maxBalCurrent min12(Bal1I, Bal2I, Bal3I, Bal4I, Bal5I, Bal6I, Bal7I, Bal8I, Bal9I, Bal10I, Bal11I, Bal12I)

#define minCount UINT8_MAX
//these constants are derived like this.  Let D be the duty cycle we want
//What's asked for is D * minBalCurrent.  What's really done is D * BalxI
//So, we want to adjust the max count to make these equal, effectively reducing D
//proportionally to how much larger the balancer current is than the minimum.
//so, we get
// D * k_x * BalxI = D * minBalCurrent
//k_x = minBalCurrent / BalxI
//so, make the period for all balancers, minCount / k_x = minCount * BalxI / minBalCurrent



//#define AllowChargeOverDischargedCells
#ifdef AllowChargeOverDischargedCells
#pragma message "Cell undervoltage is disabled.  Cell damage could result."
#endif

//#define IgnoreErrors
#ifdef IgnoreErrors
#pragma message "Ignore Errors is defined.  Cell damage could result."
#endif
//PFETS IRF9540N
//Diodes CMSH5-40

/*
 * For the Mega 16, the pin assignments will be
 * PA - free for now (that's all the ADC channels)
 * PC0-PC5: JTAG
 * PB4-PB7: SPI, for LT6803, SS:PB4, MOSI:PB5, MISO:PB6, SCK:PB7
 * PD0-PD1: RS-232. for PC comm
 * PB0: Stack disable
 * Free ports:
 *   PD2-PD7
 *   PB1-PB3 
 *   PC6-PC7
 */
#define DisableStackPort PORTB
#define DisableStackPin	PB0
#define TriggerPort PORTB
#define TriggerPin PB1

#define ErrorLED PD2
#define ErrorLEDPort PORTD
#define BalanceLED PD3
#define BalanceLEDPort PORTD
#define CalibrateLED PD4
#define CalibrateLEDPort PORTD
#define BalancerDisconnect PB2
#define BalancerDisconnectPort PORTB

enum ErrorCodes{
	NoError=1,
	GeneralError = 10,
	WatchdogBit,
	LogError,
	
	LT6802TransferError = 25,
	LT6802CellOVUVError,
	LT6802CommunicationsError,
	LT6802TimeoutError,
	
	SoftCheckError = 50,
	SoftCheckUVError,
	SoftCheckOVError,
	SoftCheckTimeout,
	
	AlgorithmError = 75,
	AlgorithmErrorDefaultMode,
	AlgorithmErrorDefaultState,
	AlgorithmErrorCVCharge,
	AlgorithmErrorCalBalanceUV,
	
	ActiveBalanceError = 80,
	ActiveBalanceNoSuchMode,
};

#endif