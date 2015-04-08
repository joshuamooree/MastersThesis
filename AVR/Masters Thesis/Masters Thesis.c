/*
 * Masters_Thesis.c
 *
 * Created: 1/15/2012 2:31:06 PM
 *  Author: A
 * 
 * For the Mega 16, the pin assignments will be
 * PA - free for now (that's all the ADC channels)
 * PC0-PC5: JTAG
 * PB4-PB7: SPI, for LT6803, SS:PB4, MOSI:PB5, MISO:PB6, SCK:PB7
 * PD0-PD1: RS-232. for PC comm
 * PB0: Stack Disable
 * Free ports:
 *   PD2-PD7
 *   PB1-PB3 
 *   PC6-PC7
 */ 

#include "Masters Thesis.h"
#include <avr/io.h>
#include <util/delay.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <stddef.h>

#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include "Algorithm.h"

//#include "setupBlockingUARTOutput.h"
#include "setupUARTOutput.h"



//volatile uint8_t PINBstatus;

volatile uint16_t DischargerStatus;	//bit field representing state of each discharger.  1 = on,, 0 = off
volatile int32_t currentAccumulators [NumberOfCells] = {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0};
volatile int16_t stackCurrent = 0x0000;
//static volatile int16_t cellCurrentAdjust [NumberOfCells] = {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0};
volatile uint8_t oneSecondPassed;
volatile uint8_t hundredMSPassed;
volatile uint32_t time=0;
volatile uint8_t NewADCReadings = 0x00;
volatile uint8_t NewISenseReadings = 0x00;
volatile uint8_t globalError = NoError;
volatile uint8_t balanceMode = normalDisch;
volatile uint8_t balancePWMThresholds [NumberOfCells];
volatile bool force6802Conversion = false;
volatile bool inhibitStackIADC = false;

//units are a bit strange:
//      LT6802ADCTicks (1.5mV/tick)
// R1 = ------------------------------- / 100= 6 mohms/tick
//      Current ADC ticks (1A/CurrentTick ticks)
uint16_t R [NumberOfCells];
uint16_t R1 [NumberOfCells];
uint16_t C1 [NumberOfCells] = {	UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
								UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
								UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX};
double R1I [NumberOfCells] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
int32_t capacities [NumberOfCells];

volatile SPIStatusStruct SPIStatus = {false, NULL, 0, 0, false};
volatile bool updateCFGReg=false;
uint8_t logMode = logRaw;

setupOutputDevice

void init()
{
	//initialize the SPI port
	
	//PB5 is MOSI, PB7 is SCK - these need to be outputs
	//PB4 is SS (also output)
	DDRB = (1 << PB4) | (1 << PB5) | (1 << PB7) | (1 << PB4) | (1 << DisableStackPin) | (1 << BalancerDisconnect) | (1 << TriggerPin);
	//enable SPI, setup as master
	//together this line and the next setup data comm at f_osc/8
	//also, enable SPI interrupts
	//SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPIE) | (1 << SPR0);
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPIE) | (1 << SPR1) | (1 << CPHA) | (1 << CPOL);
	SPSR = (1 << SPI2X);
	//start SS high
	PORTB |= (1 << PB4);
	
	//setup timer counter 1
	//no need to touch waveform generation mode - default is Normal
	//setup clock to be clk/1024
	TCCR1B = (1 << CS12) | (1 << CS10);
	//enable the timer overflow interrupt and output compare channel A ISR
	TIMSK = (1 << TOIE1) | (1 << OCIE1A) | (1 << OCIE1B);
	//TIMSK = (1 << OCIE1A) | (1 << OCIE1B);
	uint16_t time = TCNT1;
	OCR1A = time + Timer1OneSecondOffset;
	
	//setup ADC
	//enable ADC, allow the ADC to run as fast as possible
	ADCSRA = (1 << ADEN);

	//enable conversion from internal 2.56V reference.  Use ADC0 for input (single ended)
	ADMUX=(1 << REFS1) | (1 << REFS0);
	//don't touch SFIOR to allow free running mode
	
	//setup LED ports
	DDRD = (1 << ErrorLED) | (1 << BalanceLED) | (1 << CalibrateLED);
	PORTD |= (1 << ErrorLED) | (1 << BalanceLED) | (1 << CalibrateLED);
	

	initializeOutputDevice
	
	sei();	//allow interrupts
}


int main(void)
{
	#ifndef IgnoreErrors
	//watchdog stuff
	if(MCUCSR & (1<<WDRF))
	{
		//oops we got here because the watchdog bit
		init();
		emergencyShutdown();
		globalError = WatchdogBit;
		puts_P(PSTR("**Watchdog bit, stopping**\n"));
		while(true)
			;
	}
	#endif
	
	MCUCSR &= ~(1<<WDRF);
	wdt_disable();
	
	//initialization
	init();
	
	ErrorLEDPort &= ~(1 << ErrorLED);
	BalanceLEDPort &= ~(1 << BalanceLED);
	CalibrateLEDPort &= ~(1 << CalibrateLED);
	
	//enable stack current
	DisableStackPort |= (1 << DisableStackPin);
	BalancerDisconnectPort |= (1 << BalancerDisconnect);

	//start the first ADC conversion
	ADCSRA |= (1 << ADSC);
	
	CVRegPacket6802 voltageRegisters;
	
	//temporary - turn power supply on so we can start charging
	//set the power supply to 60V, and current to 0.5A
	//since we charge batteries, the supply should be current limited)
	//puts("<ps<P50.9VC0.5ARG>>\n");
	puts_P(PSTR("\n"));
	
	AlgorithmState state;
	//state.mode=JustLog;
	//state.mode = Test;
	//state.mode=FindCapacities;
	//state.mode=ChargeCells;
	//state.mode=ExtractParams;
	//state.mode=DoTopBalance;
	state.mode=ActiveBalanceChargeDischarge;
	
	state.voltagePacket=&voltageRegisters;
	
	//setup thread variables
	threadVar algorithmThread;
	algorithmThread.state=&state;
	PT_INIT(&algorithmThread.ptVar);
	
	logThreadVars logThread;
	logThread.voltageRegisters=&voltageRegisters;
	PT_INIT(&logThread.ptVar);
	
	commThreadVars comThread;
	comThread.voltageRegisters=&voltageRegisters;
	PT_INIT(&comThread.ptVar);
	
	commThreadVars voltageCheckThread;
	voltageCheckThread.voltageRegisters=&voltageRegisters;
	PT_INIT(&voltageCheckThread.ptVar);
	
	struct pt stackCurrentThread;
	PT_INIT(&stackCurrentThread);
	
	struct pt updateOCVThread;
	PT_INIT(&updateOCVThread);
	
	struct pt balanceMonitorThread;
	PT_INIT(&balanceMonitorThread);
	
	doneDischarging(state.voltagePacket, 25);
	
	while(time < 1)
		;
	
	wdt_reset();
	#ifndef IgnoreErrors
	wdt_enable(WDTO_1S);
	#endif
	
	ErrorLEDPort |= (1 << ErrorLED);
	BalanceLEDPort |= (1 << BalanceLED);
	CalibrateLEDPort |= (1 << CalibrateLED);
	
	puts_P(PSTR("**Init done.**\n"));
	
	#ifdef IgnoreErrors
	puts_P(PSTR("**Warning: Error checks disabled**\n"));
	#endif
	
	
    while(1)
    {	 
		wdt_reset();

		//don't run the algorithm if we hit an error state - no point
		#ifndef IgnoreErrors
		if(globalError == NoError)
		{
		#endif
			PT_SCHEDULE(LT6802CommThread(&comThread));
			PT_SCHEDULE(SoftVoltageCheck(&voltageCheckThread));
			PT_SCHEDULE(ReadStackCurrent(&stackCurrentThread));
			PT_SCHEDULE(updateOCV(&updateOCVThread));
			PT_SCHEDULE(masterThread(&algorithmThread));
			PT_SCHEDULE(activeBalanceMonitor(&balanceMonitorThread));
			PT_SCHEDULE(logData(&logThread));
		#ifndef IgnoreErrors
		} else {
			emergencyShutdown();
			printf_P(PSTR("**Loop halted at %5"PRIu32": Error code: %d**\n"), time, globalError);
			puts_P(PSTR("**Exit**\n"));
			NewADCReadings = 0xFF;
			PT_SCHEDULE(LT6802CommThread(&comThread));
			NewADCReadings = 0xFF;
			NewISenseReadings = 0xFF;
			PT_SCHEDULE(logData(&logThread));
			while(true)
				wdt_reset();	//we already caught the error - just keep resetting the watchdog
		}
		#endif
    }	//end main loop
}


ISR(SPI_STC_vect)
{
	//get the data we got back from the SPI transfer
	if(SPIStatus.readDataFromSPI)
	{
		SPIStatus.Data[SPIStatus.currentByte]=SPDR;
	}
	SPIStatus.currentByte++;
	
	//if we've got stuff to send, send it
	//else, we're done - set SS low and set TransferInProgress to false
	if(SPIStatus.currentByte < SPIStatus.DataLength)
	{
		SPDR=SPIStatus.Data[SPIStatus.currentByte];
	} else {
		uint8_t PINBstatus = PINB;
		if(SPIStatus.readSDOStatusBeforeCS)
		{
			SPIStatus.SDOStatus=((PINBstatus & (1 << PB6)) >> PB6);
		}
		//disable SS
		PORTB |= (1 << PB4);
		SPIStatus.TransferInProgress=false;
	}
}


ISR(TIMER1_COMPA_vect)
{
	//get the time for the interrupt, then set the compare
	//to 1 second from now
	uint16_t timer = TCNT1;
	OCR1A = timer + Timer1OneSecondOffset;
	time++;
	oneSecondPassed = 0xFF;
	
	//accumulate current
	//we will measure capacity in ADC ticks * second
	//we have seconds because that's the period of this ISR
	for (uint8_t i = 0; i < NumberOfCells; i++)
	{
		currentAccumulators[i] += stackCurrent;// add balancer current every second
	}		
}

ISR(TIMER1_OVF_vect)
{
//	DischargerStatus = 0x0000;
//	updateCFGReg = true;
}



ISR(TIMER1_COMPB_vect)
{
	/*
	These constants work like this:
	  The pwm period for the smallest balancer is uint8_max *1/10 seconds
	  Any larger balancers have their PWM period increased correspondingly
	    Note: this means that larger balancers cannot reach 100% DC.  It also
		means that the balancers may not be in sync.
	  so,
	                 i_bal_i 
	  pwmCntMax_i = ---------- * uint8_max
	                 i_bal_min
	*/
	const uint16_t balLengths [NumberOfCells] = {(uint16_t)((minCount * Bal1I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal2I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal3I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal4I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal5I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal6I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal7I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal8I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal9I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal10I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal11I) / (minBalCurrent)),
												 (uint16_t)((minCount * Bal12I) / (minBalCurrent))};
		
	
	uint16_t timer = TCNT1;
	OCR1B = timer + HundredMSOffset;
	hundredMSPassed = 0xFF;
	
		
	static uint16_t balancePWMCount[NumberOfCells] = {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0};
		
	if(balanceMode == PWMDisch)
	{	
		uint16_t status = 0x0000;
		for(uint8_t i = 0; i < NumberOfCells; i++)
		{
			if(balancePWMCount[i] < balLengths[i])
				balancePWMCount[i]++;
			else
				balancePWMCount[i] = 0;
			
			if(balancePWMCount[i] < balancePWMThresholds[i])
			{
				status |= (1 << i);
			}
		}
		DischargerStatus = status;
	}
	
	for(uint8_t i = 0; i < NumberOfCells; i++)
	{
		//divide by 10 because we update this 10x/second
		if(isSet(i, DischargerStatus))
		{
			currentAccumulators[i] -= (int32_t)(BalancerCurrent*CurrentTick/10);
		}
	}		
}

void startSPITransaction (uint8_t* data, uint8_t size, uint8_t flags)
{
	SPIStatus.Data = data;
	SPIStatus.DataLength = size;
	SPIStatus.SDOStatus = 0;
	SPIStatus.readSDOStatusBeforeCS = !((flags & SDOStatusBeforeCS)==0);
	SPIStatus.TransferInProgress = true;
	SPIStatus.currentByte = 0;
	SPIStatus.readDataFromSPI = !((flags & readFromSPI)==0);
	//enable SS
	PORTB &= ~(1 << PB4);
	
	SPDR = data[0];
}

enum TransferState
{
	sendSetupData = 1,
	preConversion,
	pollingADC,
	pollingInterrupt,
	gotInterrupt,
	gotFlagData,
	gotADCData,
	error
};

PT_THREAD(LT6802CommThread(commThreadVars* vars))
{
	static uint8_t LT6802State = sendSetupData;
	static CfgRegPacket6802 configRegisters;
	static AddressCommandPacket addressCommPacket;
	static FLGRegPacket6802 flagRegisters;
	static uint8_t badDataCount = 0;
	static uint8_t errorCount = 0;
	static uint8_t stoppedBalancers = false;
	static uint8_t hundredMSCount =0;
	static uint32_t ADCTimoutTime=0;
	
	
	
	PT_BEGIN(&vars->ptVar);

	configRegisters.packet.payload[0].reg.CFGR0=(1 << LVLPL) | (1 << CDC0);//(1 << CDC2) | (1 << CDC0);
	configRegisters.packet.payload[0].reg.CFGR1=0;

	configRegisters.packet.payload[0].reg.CFGR2=((MaskedCells & 0x000F)<<4);
	//configRegisters.packet.payload[0].reg.CFGR2=0;	//don't mask interrupts
	//configRegisters.packet.payload[0].reg.CFGR3=0;	//don't mask interrupts
	configRegisters.packet.payload[0].reg.CFGR3=((MaskedCells & 0x0FF0)>>4);
	
	configRegisters.packet.payload[0].reg.VOV=(uint8_t)(alarmOV/16);
	//configRegisters.packet.payload[0].reg.VOV=0xFF;
	
	#ifndef AllowChargeOverDischargedCells
	configRegisters.packet.payload[0].reg.VUV=(uint8_t)(alarmUV/16);
	#else
	configRegisters.packet.payload[0].reg.VUV=0x00;
	#endif

	//ok, we have the config register setup, now setup and begin the transfer
	configRegisters.packet.command = WRCFG;
	
	while(true)
	{
		if(!SPIStatus.TransferInProgress)
		{
			//we have a state machine here
			//state 1: send setup info
			//state 2: start the ADC - send go packet, if its that time, otherwise, poll interrupt
			//state 3: poll for data
			//state 4: poll for interrupt - send packet to get flag registers
			//state 5: check flag register, if all is well, get ADC data, if not, we are in error
			//state 6: we got the ADC data - notify the rest of the main loop we have new data, goto state 1
			//state 7: error state - send signal to stop charging/discharging, turn off balancers, set error light
			switch(LT6802State)
			{
				case sendSetupData:

					
					configRegisters.packet.command = WRCFG;
					startSPITransaction(configRegisters.bytes, sizeof(configRegisters.bytes), noFlags);
					LT6802State= preConversion;
					break;
				case preConversion:
					
					if (updateCFGReg) {
						updateCFGReg = false;
						
						//kill the dischargers if we disable the UV check
						#ifdef AllowChargeOverDischargedCells
						DischargerStatus = 0x0000;
						#endif
						//set the discharger status registers based on our bitmap
						configRegisters.packet.payload[0].reg.CFGR1=(DischargerStatus & 0x00FF);
						configRegisters.packet.payload[0].reg.CFGR2 &= 0xF0;
						configRegisters.packet.payload[0].reg.CFGR2 |= ((DischargerStatus & 0x0F00) >> 8);
						LT6802State = sendSetupData;
					//poll the ADC once per second
					}else if((oneSecondPassed & secondReadMask) || force6802Conversion)	
					{
						hundredMSPassed &= ~secondReadMask;
						force6802Conversion = false;
						hundredMSCount=0;
						
						
						//if(!stoppedBalancers)
						//{
						//	stoppedBalancers = true;
						//	configRegisters.packet.payload[0].reg.CFGR1=0x00;
						//	configRegisters.packet.payload[0].reg.CFGR2 &= 0xF0;
						//	LT6802State = sendSetupData;
						//} else {
							stoppedBalancers = false;
							//commandPacket.reg.command=STCVADAll;
							inhibitStackIADC = true;
							oneSecondPassed &= ~secondReadMask;
							addressCommPacket.reg.command=STCVADAll;
							addressCommPacket.reg.address=0x85;
							startSPITransaction(addressCommPacket.bytes, sizeof(addressCommPacket.bytes), noFlags);
							LT6802State = pollingADC;
							
							#ifndef IgnoreErrors
							ADCTimoutTime=time;
							#endif
						//}							
					
					//poll the interrupt every hundred ms
					} else if(hundredMSPassed & secondReadMask){
						hundredMSCount++;
						hundredMSPassed &= ~ secondReadMask;
						if (hundredMSCount > 9)
						{
							configRegisters.packet.payload[0].reg.CFGR1=0x00;
							configRegisters.packet.payload[0].reg.CFGR2 &= 0xF0;
							LT6802State = sendSetupData;
							//puts_P(PSTR("**bals off**\n"))	;
						} else {						
							addressCommPacket.reg.command=PLINT;
							addressCommPacket.reg.address=0x85;
							startSPITransaction(addressCommPacket.bytes, sizeof(addressCommPacket.bytes), SDOStatusBeforeCS);
							LT6802State = pollingInterrupt;
						}

					}
					break;
				case pollingInterrupt:
				
					//We want to resend the PLADC packet if the SDO line didn't indicate the conversion is done
					//low for busy / in interrupt
					//LT6802 interrupts if all the cells are within range, but interrupt is indicated by 
					//sdo low (same as busy).  So, low still means keep polling - nothing interesting to
					//report.
					//We check the condition here since last packet sent was poll
					//interrupt (and the poll interrupt state checks poll adc stuff)
					if(SPIStatus.SDOStatus)
					{
						//everything is good if the pin is low
						LT6802State = preConversion;
					} else {
						//this is a little odd here since thats the one for interrupts, but
						//the last poll we did was for interrupt, so, if we got interrupted,
						//we want to go to the interrupt stuff
						flagRegisters.reg.command=RDFLG;
						flagRegisters.reg.address=0x85;
						startSPITransaction(flagRegisters.bytes, sizeof(flagRegisters.bytes), readFromSPI);
						LT6802State = gotFlagData;
					}
					break;
					
				case pollingADC:
					//low for busy / in interrupt
					if(!SPIStatus.SDOStatus)
					{
						//addressCommPacket.reg.command=PLINT;
						addressCommPacket.reg.command=PLADC;
						addressCommPacket.reg.address=0x85;
						startSPITransaction(addressCommPacket.bytes, sizeof(addressCommPacket.bytes), SDOStatusBeforeCS);
						LT6802State = pollingADC;
						
						#ifndef IgnoreErrors
						#define maxADCPollTime 5
						if((time-ADCTimoutTime) > maxADCPollTime)
						{
							puts_P(PSTR("**Timeout polling for ADC response**\n"));
							globalError=LT6802TimeoutError;
						}
						#endif
					} else {
						vars->voltageRegisters->reg.address=0x85;
						vars->voltageRegisters->reg.command=RDCV;
						startSPITransaction(vars->voltageRegisters->bytes, sizeof(vars->voltageRegisters->bytes), readFromSPI);	
						LT6802State = gotADCData;
					}
					break;
				case gotFlagData:
					//if none of the flags are set, we're good to get the ADC readings
					if (flagRegisters.reg.packet.reg.FLGR0 == 0 &&
						flagRegisters.reg.packet.reg.FLGR1 == 0 &&
						flagRegisters.reg.packet.reg.FLGR2 == 0)
					{
						//? what are we doing here?  We shouldn't be in this state unless there was a problem.
						//print an error and reset the state
						puts_P(PSTR("**Error: Unexpected interrupt**\n"));
						LT6802State = sendSetupData;
					} else {
						if(time > 5 && errorCount > 5)	//give a grace period
						{
							#ifndef IgnoreErrors
							emergencyShutdown();
						
							//light the error LED
							ErrorLEDPort = ~(1 << ErrorLED);
							//send a nice error message
							puts_P(PSTR("**Error some cell has under or over voltaged**\n"));
							oneSecondPassed = 0x00;		//suppress possible next log
							
							globalError = LT6802CellOVUVError;
							LT6802State = error;
							#else
							if(time % 10 == 1)
								LT6802State = sendSetupData;
							#endif
						} else {
							//let's try this again and see if things are OK
							errorCount++;
							LT6802State = sendSetupData;
						}
					}
					break;

				case gotADCData:
					if(CRC86802(vars->voltageRegisters->reg.payload[0].bytes, sizeof(CVRReg6802Struct)-1) 
						!= vars->voltageRegisters->reg.payload[0].reg.PEC)
					{
						TriggerPort ^= (1 << TriggerPin);
						puts_P(PSTR("**Warning: Pec mismatch**\n"));
						#ifndef IgnoreErrors
						if(badDataCount > 10)
						{
							globalError= LT6802CommunicationsError;
							emergencyShutdown();
						}
						#endif
						badDataCount++;
						NewADCReadings = 0x00;					//this reading was bad.  don't let anybody use it.
						oneSecondPassed |= secondReadMask;		//trick into getting another reading
						LT6802State = preConversion;
						break;
					}
					errorCount = 0;
					badDataCount=0;
					//crc=CRC86802(voltageRegisters.reg.payload[0].bytes, sizeof(voltageRegisters.reg.payload[0].bytes)-1);
					NewADCReadings = 0xFF;
					inhibitStackIADC = false;
					//updateCFGReg = true;
					//addressCommPacket.reg.command=PLINT;
					//addressCommPacket.reg.address=0x85;
					//startSPITransaction(addressCommPacket.bytes, sizeof(addressCommPacket.bytes), SDOStatusBeforeCS);
					
					updateCFGReg = true;
					LT6802State = preConversion;
					break;
				case error:
					//what to do, what to do?  For now, let's just stay in this state
					break;
					
			}	//end switch
		}	//end if
		PT_YIELD(&vars->ptVar);
	}	//end while
	
	PT_END(&vars->ptVar);
}

PT_THREAD(logData(logThreadVars* var))
{
	static uint8_t prevLogMode = 0x00;
	PT_BEGIN(&var->ptVar);
	
	while(true)
	{
		if(logMode != prevLogMode)
		{
			prevLogMode = logMode;
			printf_P(PSTR("**Log mode: %"PRIx8"**\n"), logMode);
		}
		//optional stuff in case we decide to poll the LT6802 more often than once / second
		//if(oneSecondPassed & secondPrintMask)
		//{
		//	oneSecondPassed &= ~secondPrintMask;
		
		if(/*oneSecondPassed & secondPrintMask &&*/ 
			NewADCReadings & ADCPrint && 
			NewISenseReadings & ISensePrint && 
			logMode != logNone)
		{
			oneSecondPassed &= ~secondPrintMask;
			NewADCReadings &= ~ADCPrint;
			NewISenseReadings &= ~ISensePrint;
			
			//int16_t count;
			printf_P (PSTR("%5" PRIu32 "\t"), time);
			if(logMode & logBalancer)
			{
				printf("%"PRIx16"\t", DischargerStatus);
			}
			if(logMode & logStackI)
			{
				printf("%"PRId16"\t", stackCurrent);
			}
			
			for(int i = 1; i <= NumberOfCells; i++)
			{
				if(logMode & logRaw)
				{
					print680xCV(voltageFromCVReg(var->voltageRegisters->reg.payload[0].bytes, i));
				}
				if(logMode & logOCV){
					print680xCV(getOCV(var->voltageRegisters, i));
				}
				/*
				if(logMode & logSOC){
					puts_P(PSTR("**fixme**\n"));		
					globalError = LogError;
				}
				*/
				if(logMode & logR1I)
				{
					printf_P(PSTR("%"PRId16"\t"), (int16_t)R1I[i-1]);
				}
				if(logMode & logPWMThresh)
				{
					printf_P(PSTR("%"PRIu8"\t"), balancePWMThresholds[i-1]);
				}
							
				//each count of current accumulator is worth
				//20 A / 1024 = 19.53 mA-s - well call it 20mA - s
				//we are reporting A - seconds and there are 50 20mA per A
				//printf("\t%4"PRId32".%02"PRId32"\t", currentAccumulators[i-1]/CurrentTick, 
				//							(currentAccumulators[i-1] >= 0 ? 
				//								(currentAccumulators[i-1]%CurrentTick)*100 :
				//								((-currentAccumulators[i-1])%CurrentTick))/CurrentTick);
				printf_P(PSTR("%6"PRId32"\t"), currentAccumulators[i-1]/CurrentTick);
			}
			
			printf_P(PSTR("\n"));
			//we used the ADC readings, now we are done with them
		}			
	
		PT_YIELD(&var->ptVar);
	}	
	
	PT_END(&var->ptVar);
}

PT_THREAD(SoftVoltageCheck(commThreadVars* vars))
{
	
	static int16_t reading;
	#ifndef IgnoreErrors
	static uint8_t errorCount=0;
	static uint32_t lastTime;
	#endif
	
	PT_BEGIN(&vars->ptVar);
	#ifndef IgnoreErrors
	lastTime=time;
	#endif
	
	while (true)
	{
		if((NewADCReadings & ADCGlobalAlarm) && (time > 5))
		{
			NewADCReadings &= ~ADCGlobalAlarm;
			
			#ifndef IgnoreErrors
			lastTime=time;
			#endif
			
			for(int i = 1; i <= NumberOfCells; i++)
			{
				//Test the index against the Masked cells
				//take 1 left shift by i-1 to get the bitfield for
				//that cell then and with the cell mask if the result
				//is 1, we don't want to check that cell
				if(!((1 << (i-1)) & MaskedCells))
				{
					reading = voltageFromCVReg(vars->voltageRegisters->reg.payload[0].bytes, i);
					#ifndef IgnoreErrors
					
					#ifndef AllowChargeOverDischargedCells
					if((reading < alarmUV) || 
						(reading > alarmOV))
					#else
					if(reading > alarmOV)
					#endif
					{
						if(errorCount > 3)
						{
							emergencyShutdown();
							//light the error LED
							ErrorLEDPort = ~(1 << ErrorLED);
							//send a nice error message
							puts_P(PSTR("**Error some cell has under or over voltaged**\n"));
							
						#ifndef AllowChargeOverDischargedCells
							if(reading < alarmUV)
								globalError = SoftCheckUVError;
							else
						#endif
								globalError = SoftCheckOVError;
							
							oneSecondPassed = 0x00;		//suppress possible next log
						} else {
							errorCount++;
						}
						goto end;
					}
					#endif
				}			
			}	//end for
			#ifndef IgnoreErrors
			errorCount = 0;
			#endif
		}	//end new adc reading if
		end:
		
		//make sure the voltage check runs with at least a certain frequency
		#ifndef IgnoreErrors
		#define SoftCheckTimeoutSeconds 30
		if(((time - lastTime) > SoftCheckTimeoutSeconds) && time > 6)
		{
			
			globalError = SoftCheckTimeout;
		}
		#endif
		
		PT_YIELD(&vars->ptVar);
	}	//end while
	PT_END(&vars->ptVar);
}

enum ADCModes
{
	ReadADC0 = 1,
	ReadADC1,
};

PT_THREAD(ReadStackCurrent(struct pt* ptVars))
{
	static int16_t ADC0 = 0;
	static int16_t ADC1 = 0;
	static uint8_t ADCState;
	
	PT_BEGIN(ptVars);
	
	ADCState = ReadADC0;
	ADC0 = 0;
	ADC1 = 0;
	stackCurrent = 0;
	
	//wait for the ADC to settle out before we start trying to read data
	PT_WAIT_UNTIL(ptVars, time > 1);
	
	while(true)
	{
		//if we are inhibiting the ADC readings, we want to reset everything and start over
		if(inhibitStackIADC)
		{
			//NewISenseReadings = 0x00;
			ADCState = ReadADC0;
			ADMUX = (ADMUX & (0xE0)) | 0;
			PT_WAIT_WHILE(ptVars, inhibitStackIADC);
			ADCSRA &= ~(1 << ADIF);  //clear conversion
			ADCSRA |= (1 << ADSC);	//start another conversion
		}
		else if(ADCSRA & (1 << ADIF))
		{
			int32_t ADCTemp;
			ADCSRA &= ~(1 << ADIF);
			//if last time we had state ReadADC0, this reading was for that
			if(ADCState == ReadADC0)
			{
				ADCTemp = ADC;
				ADCTemp=(((uint32_t)ADCTemp) * IMeasCalPosNum)/IMeasCalPosDem;
				ADC0 = (uint16_t)ADCTemp;
				
				ADCState = ReadADC1;
				ADMUX = (ADMUX & (0xE0)) | 1;
			} else {
				ADCTemp = ADC;
				//ADCTemp = (((uint32_t)ADCTemp)* IMeasCalNegNum)/IMeasCalNegDem;
				ADC1 = (uint16_t)ADCTemp;
				
				//we set NewISenseReadings here since that way we only update after we've read both positive and negative
				NewISenseReadings = 0xFF;
				
				ADCState = ReadADC0;
				ADMUX = (ADMUX & (0xE0)) | 0;
			}
			_delay_us(5);
						
			ADCSRA |= (1 << ADSC);	//start another conversion
		}

		if(NewISenseReadings & IsenseFlag)
		{
			NewISenseReadings &= ~IsenseFlag;
			
			#define negativeThreshold 0x02
			if(ADC0 >= negativeThreshold)
			{
				stackCurrent = ADC0;
				ADC1 = 0;
			}			
			else if(ADC1 >= negativeThreshold)
			{
				stackCurrent = -ADC1;
				ADC0 = 0;
			}			
			else
				stackCurrent = 0;	//undefined case - both are 0
		}
				
					
			
		PT_YIELD(ptVars);
	}	//end while
	
	PT_END(ptVars);
}
