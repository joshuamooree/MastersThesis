#include "Algorithm.h"
#include <float.h>

uint16_t EEMEM REEPROM [NumberOfCells];
uint16_t EEMEM R1EEPROM [NumberOfCells];
uint16_t EEMEM C1EEPROM [NumberOfCells];
uint16_t EEMEM capEEPROM[NumberOfCells];

int16_t BDCDivide[NumberOfCells];	//numerator is 10,000
uint8_t ActiveBalanceMode = ActiveBalanceOff;

void initEEPROM()
{
		eeprom_read_block(R, REEPROM, NumberOfCells*sizeof(uint16_t));
		eeprom_read_block(R1, R1EEPROM, NumberOfCells*sizeof(uint16_t));
		eeprom_read_block(C1, C1EEPROM, NumberOfCells*sizeof(uint16_t));
		eeprom_read_block(capacities, capEEPROM, NumberOfCells*sizeof(uint16_t));
}

enum calibrateStates
{
	CalibrateTopBalance,
	CalibrateBottomBalance,
	BalanceDone
};

#ifdef multiCellBalance
bool calibrateBalance(threadVar* var, uint8_t mode, uint8_t tolerance)
{
	#ifndef IgnoreErrors
	static uint8_t errorCount = 0;
	#endif
	
	//OK, we're done discharging, now we want to shuffle current around until
	//all the cells have the same voltage within the tolerance
	if(NewADCReadings & ADCBalanceFlag && NewISenseReadings & CalBalance)
	{
		NewADCReadings &= ~ADCBalanceFlag;
		NewISenseReadings &= ~CalBalance;
				
		int16_t minCellV = INT16_MAX;
		int16_t maxCellV = INT16_MIN;
		int16_t cellVoltage;
		uint8_t minCell = NumberOfCells;		

		for(uint8_t i = 1; i <= NumberOfCells; i++)
		{
			//cellVoltage = voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, i);
			cellVoltage = getOCV(var->state->voltagePacket, i);
			if(cellVoltage < minCellV)
			{
				minCellV = cellVoltage;
				minCell = i;
			}
			if(cellVoltage > maxCellV)
			{
				maxCellV = cellVoltage;
			}
		}
		
		#ifndef IgnoreErrors
		//we want to ignore transient measurement errors.  So, only stop if we've seen 
		//5 consecutive low readings.
		if(mode == CalibrateTopBalance && minCellV < cellUV/LT6802CountValue)
		{
			if(errorCount > 4)
			{
				globalError = AlgorithmErrorCalBalanceUV;
				return false;
			} else {
				errorCount++;
			}
		} else {
			errorCount = 0;
		}
		#endif
		
		DischargerStatus = 0x0000;
		for(uint8_t i = 1; i <= NumberOfCells; i++)
		{
			if((i != minCell) && 
					((getOCV(var->state->voltagePacket, i) >= (maxCellV+minCellV)/2)) &&
					((getOCV(var->state->voltagePacket, i) > (minCellV + tolerance))))
			{
				//if(abs(getOCV(var->state->voltagePacket, i) - getOCV(var->state->voltagePacket, minCell)) > 5)
				//{
					DischargerStatus |= (uint16_t)(1 << (i-1));
					//now that we turned on a balancer, we need to force a new conversion
					//and tell everyone else to ignore the old stuff
					force6802Conversion = true;
					NewADCReadings = 0x00;
					NewISenseReadings = 0x00;
				//}					
			}
		}

		updateCFGReg = true;
					
		if(DischargerStatus == 0x00)
		{
			return true;
		}	
	}
	return false;
}
#else
bool calibrateBalance(threadVar* var, uint8_t mode, uint8_t tolerance)
{	
	static uint8_t maxCellIndex;
	static uint8_t minCellIndex;
		
			
	//OK, we're done discharging, now we want to shuffle current around until
	//all the cells have the same voltage within the tolerance
	if(NewADCReadings & ADCBalanceFlag && NewISenseReadings & CalBalance)
	{
		NewADCReadings &= ~ADCBalanceFlag;
		NewISenseReadings &= ~CalBalance;
				
		int16_t minCellV = INT16_MAX;
		int16_t maxCellV = 0;
		int16_t cellVoltage;
				
		//basically we aren't balancing
		if(DischargerStatus == 0)
		{
			maxCellIndex = NumberOfCells;
			minCellIndex = NumberOfCells;
		}						

		//if(voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, maxCellIndex+1) > 
		//   voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, minCellIndex+1))
		if(getOCV(var->state->voltagePacket, maxCellIndex+1) > 
			getOCV(var->state->voltagePacket, minCellIndex+1))
		{
			//do nothing  - just let it keep going
		} else {
			for(uint8_t i = 1; i <= NumberOfCells; i++)
			{
				//cellVoltage = voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i);
				cellVoltage = getOCV(var->state->voltagePacket, i);
				if(cellVoltage < minCellV)
				{
					minCellV = cellVoltage;
					minCellIndex = i-1;
				}
				if(cellVoltage > maxCellV)
				{
					maxCellV = cellVoltage;
					maxCellIndex = i-1;
				}
			}				
			
			//force a new ADC Conversion and ignore the data we have now
			force6802Conversion = true;
			NewADCReadings = 0x00;
			NewISenseReadings = 0x00;
			
			if(minCellV < (maxCellV-tolerance))
			{
				DischargerStatus = (1 << maxCellIndex);
				updateCFGReg = true;
			} else {	//we found no cells to balance 
				DischargerStatus = 0;
				updateCFGReg = true;
					
				return true;
			}	
		}			
	}
	return false;
}
#endif


PT_THREAD(calibrateTopBalance(threadVar* var))
{
	PT_BEGIN(&var->ptVar);
	static threadVar subThread;
	
	static uint8_t toleranceMult = 1;
	
	subThread.state=var->state;
	
	balanceMode = normalDisch;
	BalanceLEDPort &= ~(1 << BalanceLED);
	
	while(true)
	{
		//the logic works like this
		//if we are done balancing and done charging, we are done
		//if we are done balancing, but not charging, we need to charge
		//if we are not done balancing, we need to repeat until we are
		if(calibrateBalance(var, CalibrateTopBalance, ((uint8_t)(cellOV/LT6802CountValue/InitBalanceTolerance))*toleranceMult))
		{
			PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 10));
			if(cellsBalanced(var->state->voltagePacket, cellOV/LT6802CountValue/InitBalanceTolerance*1.1))
			{
				//if(!doneCharging(var->state->voltagePacket))
				//{
				//	PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1")));
				//}
			
				BalanceLEDPort |= (1 << BalanceLED);
				puts_P(PSTR("**Calibrate Balance Done**\n"));
				//toleranceMult = 8;
				PT_EXIT(&var->ptVar);
			} else {
				//toleranceMult--;
			}
		}
		PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 1));
		
		PT_YIELD(&var->ptVar);
	}
	DischargerStatus = 0x0000;
	PT_END(&var->ptVar);
}

/*
PT_THREAD(calibrateBottomBalance(threadVar* var))
{
	static threadVar subThread;
	
	PT_BEGIN(&var->ptVar);
	static uint8_t toleranceMult = 8;
	
	subThread.state=var->state;
	
	balanceMode = normalDisch;
	BalanceLEDPort &= ~(1 << BalanceLED);
	
	while(true)
	{
		//the logic works like this
		//if we are done balancing and done discharging, we are done
		//if we are done balancing, but not discharging, we need to discharge
		//if we are not done balancing, we need to repeat until we are
		if(calibrateBalance(var, CalibrateBottomBalance, ((uint8_t)(cellOV/LT6802CountValue/InitBalanceTolerance))*toleranceMult))
		{
			if(doneCharging(var->state->voltagePacket))
			{
				BalanceLEDPort |= (1 << BalanceLED);
				puts_P(PSTR("**Calibrate Balance Done**\n"));
				PT_EXIT(&var->ptVar);
			} else {
				PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge(&subThread, PSTR("1.0")));
			}
		}
		
		PT_YIELD(&var->ptVar);
		}
	DischargerStatus = 0x0000;	
	PT_END(&var->ptVar);
}
*/

enum pulseTestStates
{
	PulseTestBegin,
	PulseTestEndPulse,
	PulseBeginSettle,
	PulseEndSettle	
};

PT_THREAD(pulseTest(threadVar* var))
{
	static uint8_t state = PulseTestBegin;
	static CVRegPacket6802 voltageRecordSettle;
	static CVRegPacket6802 voltageRecord;
	static uint32_t tStart = 0;
	static int16_t endI;
	
	PT_BEGIN(&var->ptVar);
	
	while(true)
	{
		switch(state)
		{
			case PulseTestBegin:
				//so, here's how we do our pulse test:
				//1) turn on the stack charger and set it to 1A
				//2) run that for 300 seconds
				//3) record cell voltages
				//4) turn off charger, record time
				//5) after 5 seconds, measure cell voltage again.
				//   R = (V1 - V2)/(1A/CurrentTick).  Units for that are LT6802Ticks/ISenseTicks.
				//6) Wait until cell voltage hasn't changed for 10s.  This time is ~4R_2 C1.
				//   R_2 = (V1 - V3)/(1A/CurrentTick) - R_1
				//   C1 = (time - 10 - origTime)/4/R_2 units are seconds/(LT6802Ticks/ISenseTicks)
				puts_P(PSTR("<ps<P55VC0.1ARG>>\n"));
				//puts_P(PSTR("<ps<P55VC1ARG>>\n"));
				//printPowerSupplyStackVString(state->voltagePacket, NumberOfCells, "C0.5A");
				tStart = time;
				state = PulseTestEndPulse;
				break;
			case PulseTestEndPulse:
				if((time) > (tStart+PulseWidth))
				{
					endI = stackCurrent;
					memcpy(voltageRecord.bytes, var->state->voltagePacket->bytes, sizeof(CVRegPacket6802));
					puts_P(PSTR("<ps<S>>\n"));
					tStart = time;
					NewADCReadings &= ~Algorithm;
					state = PulseBeginSettle;
				}
				break;
			case PulseBeginSettle:
				if((time) > (tStart + 1) && (NewADCReadings & Algorithm)) 
				{
					NewADCReadings &= ~Algorithm;
					
					for(int i = 1; i <= NumberOfCells; i++)
					{
						//R=(V_beforeStep-V_endStep)*RScale/I_step
						R[i-1]=((voltageFromCVReg(voltageRecord.reg.payload[0].bytes, i) - 
									 voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i))
									 *RScale)
									 /endI;
					}
					//memcpy(voltageRecordSettle.bytes, var->state->voltagePacket->bytes, sizeof(CVRegPacket6802));
					
					for(int i = 0; i < sizeof(CVRegPacket6802); i++)
						voltageRecordSettle.bytes[i] = var->state->voltagePacket->bytes[i];
					/*
					for(int i = 1; i<= NumberOfCells; i++)
					{
						printf_P(PSTR("%i, %i\n"), voltageFromCVReg(voltageRecordSettle.reg.payload[0].bytes, i),
											voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i));
						
					}						
					*/
						
					R1[0] = 0;
					
					tStart = time;
					state = PulseEndSettle;
				}
				break;
			case PulseEndSettle:
				if(NewADCReadings & Algorithm)
				{
					NewADCReadings &= ~Algorithm;
					//*state -> oneSecondPassed &= ~timeAlgorithmMask;
					
					bool allCellsSettled = true;
				
					for	(int i = 1; i <= NumberOfCells; i++)
					{
						//printf("%i\n", abs(voltageFromCVReg(voltageRecordSettle.reg.payload[0].bytes, i) - 
						//    voltageFromCVReg( var->state ->voltagePacket->reg.payload[0].bytes, i)) );
						//printf_P(PSTR("%i\n"), voltageFromCVReg(voltageRecordSettle.reg.payload[0].bytes, i));
						
						//if we've settled out (we've seen readings within ~9mV for 20s)
						//we abuse of couple of variables in here, temporarily.
						//first, we reuse C1 as a flag to indicate whether cell is being worked on or not
						//second, we reuse R1 as a counter to show number of seconds since the reading
						//last changed by less than our threshold
						//possible states:
						//1) cells settling:
						//2) cells settled, waiting for a few samples have passed with cells settled
						//3) cells settled, time has passed, constants not calculated
						//4) cells settled, constants calculated e.g. cell done
						//5) cells not settled
						if( abs(voltageFromCVReg(voltageRecordSettle.reg.payload[0].bytes, i) - 
							voltageFromCVReg(var->state ->voltagePacket->reg.payload[0].bytes, i)) 
							<= PulseSettleThreshold )
						{
							//only calculate R1 and C1 if
							//1) cell is being worked on
							//2) it been long enough since the reading last changed by less than our threshold
							if(C1[i-1] == UINT16_MAX)
							{
								if(R1[i-1] >= PulseSettleTime)
								{
									//R1=(V_beforeStep - V_settle)*RScale/I_pulse - R
									R1[i-1] =((voltageFromCVReg(voltageRecord.reg.payload[0].bytes, i)-
													 voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i))*
													 RScale)
													 /endI -
													 R[i-1];

									//tau = RC, settle in ~2RC, T=2RC
									//C=(T_current - T_setlleBegin)*CScale/(2*R1/RScale)
									C1[i-1] = ((uint16_t)(time - tStart)*(CScale*RScale/2))
													/ R1[i-1];
									printf_P(PSTR("**%"PRId8" done**\n"), i);
								} else {
									R1[i-1]++;
									allCellsSettled = false;
								}
							} //else cell done
						} else {
							//memcpy(voltageRecordSettle.bytes, state->voltagePacket->bytes, sizeof(CVRegPacket6802));
							voltageToCVReg(voltageRecordSettle.reg.payload[0].bytes, i, 
								voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i));
						
							R1[i-1] = 0;
							C1[i-1] = UINT16_MAX;
							allCellsSettled = false;
						}					
					}
					
					if(allCellsSettled)	//we're done
					{	
						//clear the voltage record field.  This field is our estimate of the internal
						//capacitor voltage
						for(int i = 1; i <= NumberOfCells; i++)
						{
							voltageToCVReg(voltageRecord.reg.payload[0].bytes, i , 0);
						}

						eeprom_update_block(R, REEPROM, NumberOfCells*sizeof(uint16_t));
						eeprom_update_block(R1, R1EEPROM, NumberOfCells*sizeof(uint16_t));
						eeprom_update_block(C1, C1EEPROM, NumberOfCells*sizeof(uint16_t));
						puts_P(PSTR("**Pulse test done**\n"));
						state = PulseTestBegin;
					
						PT_EXIT(&var->ptVar);
					}
				}			
				break;
		}			
			
		PT_YIELD(&var->ptVar);
	}		
	
	PT_END(&var->ptVar);
}

enum chargeStates
{
	Charge,
	CCCharge,
	CVCharge	
};

PT_THREAD(charge(threadVar* var, const char* current))
{
	//static uint32_t timeBegin;
	PT_BEGIN(&var->ptVar);
	static uint8_t state = Charge;
	//timeBegin = time;
	
	while(true)
	{
		switch(state)
		{
			case Charge:
				//get a good ADC reading, then just exit if cells aren't charged
				NewADCReadings &= ~Algorithm;
				
				PT_WAIT_UNTIL(&var->ptVar, (NewADCReadings & Algorithm));
				
				if(doneCharging(var->state->voltagePacket))
					PT_EXIT(&var->ptVar);
				
				printf_P(PSTR("<ps<P55VC%SARG>>\n"), current);	//set the power supply to the stack V and current
														//to 0.5A
				
				puts_P(PSTR("**Begin CC Charge**\n"));
				state = CCCharge;
				
				break;
			case CCCharge:
				DischargerStatus = 0x0000;
				updateCFGReg = true;
		
				if(NewADCReadings & Algorithm)
				{
					//enforce a minimum time for cc charge
					if(doneCharging(var->state->voltagePacket))
					{
						printPowerSupplyStackVString(var->state->voltagePacket, NumberOfCells, "C0.5A");
						state = CVCharge;
						puts_P(PSTR("**Begin CV Charge**\n"));
					} else {
						//just stay in this state - we're fine
					}
					NewADCReadings &= ~Algorithm;
				}
				break;
			case CVCharge:
				//if(doneCharging(var->state->voltagePacket))
				//{
					//the last part of the if statement prevents the CV from exiting too soon if we are close
					//to CV already
					if(stackCurrent < minChargeI)
					{
						puts_P(PSTR("<ps<S>>\n"));
						state = Charge;
						PT_EXIT(&var->ptVar);
					} else {
						//stay here
					}
				//}
				//else
				//{
					//globalError = AlgorithmErrorCVCharge;
					//PT_EXIT(&var->ptVar);
				//}
				break;
			}
			PT_YIELD(&var->ptVar);
		}		
					
	PT_END(&var->ptVar);
}

enum dischargeStates
{
	Discharge,
	DischargeMonitor
};

PT_THREAD(discharge(threadVar* var, const char* rate))
{
	PT_BEGIN(&var->ptVar);
	static uint8_t state;
	
	state = Discharge;
	
	while(true)
	{
		switch(state)
		{
			case Discharge:
				if(doneDischarging(var->state->voltagePacket))
				{
					PT_EXIT(&var->ptVar);
				}	
					
				printf_P(PSTR("<al<ISET %S>>\n"), rate);	//set discharge I
				puts_P(PSTR("<al<LOAD 1>>\n"));		//connect load
				puts_P(PSTR("**Begin Discharge**\n"));
				
				state = DischargeMonitor;
				break;
			case DischargeMonitor:
				if(NewADCReadings & Algorithm)
				{
					if(doneDischarging(var->state->voltagePacket))
					{
						puts_P(PSTR("<al<ISET 0>>\n"));	//stop discharging
						puts_P(PSTR("<al<LOAD 0>>\n"));	//disconnect load
						state = Discharge;
						PT_EXIT(&var->ptVar);
					} else {
						//just stay here
					}
				}
				break;
			}	
		PT_YIELD(&var->ptVar);
	}		
	PT_END(&var->ptVar);
}

PT_THREAD(delay(threadVar* var, uint16_t delayS))
{	
	PT_BEGIN(&var->ptVar);
	static uint16_t tBegin = 0;
	tBegin = time;
	
	PT_WAIT_WHILE(&var->ptVar, (time < (tBegin + delayS)));
	PT_EXIT(&var->ptVar);
	
	PT_END(&var->ptVar);
}

PT_THREAD(masterThread(threadVar* var))
{
	static threadVar subThread;
	//static uint16_t Qp;
	
	PT_BEGIN(&var->ptVar);
	subThread.state->voltagePacket = var->state->voltagePacket;
	
	while(true)
	{
		switch(var->state->mode)
		{
			case DoNothing:
				logMode = logNone;		
				break;
				
			case JustLog:
				initEEPROM();
				logMode = logRaw;
				
				puts_P(PSTR("**Begin just logging**\n"));
				var->state->mode = Done;
				break;
			case DischargeCells:
				PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge(&subThread, PSTR("0.1")));
				shutdown();
				var->state->mode = Done;
				break;
			case ExtractParams:
				logMode = logRaw;
				PT_WAIT_WHILE(&var->ptVar, time  <= 5);
				PT_SPAWN(&var->ptVar, &subThread.ptVar, pulseTest(&subThread));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, calibrateTopBalance(&subThread));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge(&subThread));
				
				var->state->mode = DoNothing;
				break;
		
			case DoTopBalance:
				initEEPROM();
				logMode = logRaw;
				
				puts_P(PSTR("**Begin top balance initial charge**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.2")));
				puts_P(PSTR("**Begin top balance**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, calibrateTopBalance(&subThread));
				puts_P(PSTR("**Begin final charge**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.2")));
				var->state->mode = DoNothing;
				shutdown();
			
				break;
			case ChargeCells:
				puts_P(PSTR("**Begin charge**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1")));
				puts_P(PSTR("**End Charge**\n"));
				var->state->mode = DoNothing;
				shutdown();
				break;
			case Done:
				break;
			case FindCapacities:
				//logMode = logRaw;
				logMode = logOCV | logRaw | logBalancer | logStackI | logR1I;
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 5));
				
				puts_P(PSTR("**Begin Find Capacity**\n"));
				
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.5")));
				
				while(true)
				{
					if(NewADCReadings & Algorithm)
					{
						NewADCReadings &= ~Algorithm;
						
						if(cellsBalanced(var->state->voltagePacket, cellOV/LT6802CountValue/InitBalanceTolerance*1.1))
							break;
						
						PT_SPAWN(&var->ptVar, &subThread.ptVar, calibrateTopBalance(&subThread));
						PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 30));
					}
					PT_YIELD(&var->ptVar);
				}
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1")));
				//ok, we know the cells are definitely balanced.  Now we need to reset the coulomb counters
				//and discharge
				for(uint8_t i = 0; i < NumberOfCells; i++)
				{
					currentAccumulators[i] = 0;
				}
				
				DischargerStatus = 0x00;
				
				PT_YIELD(&var->ptVar);
				
				PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge(&subThread, PSTR("0.1")));
				//ok, now we have the cells discharged and we know minimum capacity.  Rest, then
				// use our table to figure out SOC.  From there, we estimate individual capacity.
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 30));
				
				
				for(uint8_t i = 0; i < NumberOfCells; i++)
				{
					//if SOC=0, capacity should be simply the current accumulator
					//if SOC=0.25, smallest cell implied to be 75% of biggest cell, so capcity is current accumulator*1.33
					//if SOC=0.5, capacity should be current accumulator*2
					//if SOC=0.75, this implies cell is 4x bigger than smallest cell, so, capacity is current accumulator*4
					//if SOC=1, capacity should be infinite
					//so, 
					//   capacity = IAccumulator/(1-SOC)
					capacities[i] = (uint32_t)(-currentAccumulators[i])*UINT8_MAX;
					capacities[i] /= (UINT8_MAX-
						getSOC(voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i+1)));
				}
				
				PT_YIELD(&var->ptVar);
				//now store in EEPROM
				eeprom_update_block(capacities, capEEPROM, NumberOfCells*sizeof(uint16_t));
				
				//charge the pack so we don't leave it in a vulnerable place
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.5")));
				
				//finally, just do nothing
				puts_P(PSTR("**Done find capacity**\n"));
				shutdown();
				var->state->mode=DoNothing;
				break;
			case ActiveBalanceChargeDischarge:
				initEEPROM();
				calcIBDCHMult();
				ActiveBalanceMode = ActiveBalanceOn;
				
				puts_P(PSTR("**Begin Active balance**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1")));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge((&subThread), PSTR("0.1")));
				ActiveBalanceMode = ActiveBalanceOff;
				puts_P(PSTR("**Done Active Balance\n"));
				
				shutdown();
				break;
			default:
				globalError = AlgorithmErrorDefaultState;
			case AlgorithmError:
				#ifndef IgnoreErrors
				emergencyShutdown();
			
				CalibrateLEDPort &= ~(1 << CalibrateLED);	//Make sure calibrate light is on
				ErrorLEDPort &= ~(1 << ErrorLED);			//Turn on Error LED

				printf_P(PSTR("**Algorithm Error: %d**\n"), globalError);
				#endif
				break;
		}
		PT_YIELD(&var->ptVar);
	}	
	
	PT_END(&var->ptVar);
}

		/*
		case Balance:
			for (int i = 0; i < NumberOfCells; i++)
			{
				//equation for estimating desired cell I is
				//               Q_n
				//I_n = I_pack -------
				//               Q_p
				desiredCurrents[i]=(((uint32_t)packI)*capacities[i])/Qp;
			
				//equation is 
				//               desiredCurrent
				// tripPoint =   --------------- * CountMax
				//               BalancerCurrent
				//CountMax = 0xFFFF since a 16 bit counter
				//Multiplication done first in 32 bit arithmetic then divide
				//This allows us to avoid floating point math
				//we have to subtract BalancerCurrent out since our balancers
				//change the current delta - not the absolute current
				dischargerTripPoint[i] = ((((uint32_t)desiredCurrents[i])-BalancerCurrent
				)*0xFFFF)/BalancerCurrent;
			
				//turn the dischargers on or off as required
				if (TCNT1 > dischargerTripPoint[i])
					DischargerStatus &= ~(1 << i);	//clear the bit to disable the discharger, it will be set by the overflow interrupt
				
			}
			break;
*/
		
PT_THREAD(activeBalanceMonitor(struct pt* pt))
{
	static double result;
	static double maxResult = DBL_MIN;
	static double prevMaxResult = DBL_MIN;
	static double tempMaxResult;
	static int16_t balancerStackI;
	
	PT_BEGIN(pt);
	while(true)
	{
		switch(ActiveBalanceMode)
		{
			case ActiveBalanceOff:
				break;
			case ActiveBalanceEnd:
				balanceMode = normalDisch;
				ActiveBalanceMode = ActiveBalanceOff;
				break;
			case ActiveBalanceOn:
				balanceMode = PWMDisch;
				maxResult = DBL_MIN;
				prevMaxResult = DBL_MIN;
				
				//compensation for the fact that the measured stack current includes the output of the balancers
				balancerStackI = 0;
				for(uint8_t i=0; i < NumberOfCells; i++)
				{
					balancerStackI += balancePWMThresholds[i];
				}
				balancerStackI *= (BalancerCurrent/UINT8_MAX/CurrentTick);
				
				
				#define fpTolerance 0.001
				for(uint8_t i = 0; i < NumberOfCells; i++)
				{
					//the equation here works like this:
					//First, we find the desired discharge current,
					//next, we find our what percentage of our discharge current that is
					//then scale so uint8_max = 100%
					//result = ((((double)ChargeI)*10000/BDCDivide[i])/BalancerCurrent)*UINT8_MAX;
					result = ((double)(stackCurrent-balancerStackI))/CurrentTick*10000/BDCDivide[i];
					
					if(result > maxResult)
						maxResult = result;
				}
				
				while( ((prevMaxResult - maxResult) > fpTolerance) && ((prevMaxResult - maxResult) < fpTolerance) )
				{
					tempMaxResult = DBL_MIN;
					for(uint8_t i = 0; i < NumberOfCells; i++)
					{
						result = (((double)(stackCurrent-balancerStackI))/CurrentTick - 
							maxResult*(100-BalanceEff)/100.0)*10000/BDCDivide[i];
					
						if(result > tempMaxResult)
							tempMaxResult = result;
					}
					maxResult = tempMaxResult;
					PT_YIELD(pt);
				}
				
				for(uint8_t i = 0; i < NumberOfCells; i++)
				{
					balancePWMThresholds[i] = (uint8_t)(((double)(-result+maxResult)/BalancerCurrent)*UINT8_MAX);
				}
				break;
			default:
				globalError = ActiveBalanceNoSuchMode;
				#ifndef IgnoreErrors
				emergencyShutdown();
				#else
				shutdown();
				#endif
				break;
		}
		PT_YIELD(pt);
	}
	
	PT_END(pt);
}

//before calling this function, if accuracy is important, please make sure that 
//the voltage and current have been read recently
int16_t getOCV( CVRegPacket6802* voltages, uint8_t cell )
{
	//static uint32_t lastTime [NumberOfCells];

	int16_t OCV;
	
	OCV = voltageFromCVReg(voltages->reg.payload[0].bytes, cell);
	
	//terms for the series R
	OCV -= (((int32_t)R[cell-1])*stackCurrent)/RScale;
	/* DO NOT include the balancer current - balancers are turned off while the voltage
	    readings are being taken
	if(isSet(cell-1, DischargerStatus))
	{
		OCV += (((int32_t)R[cell-1])*(int16_t)(BalancerCurrent*CurrentTick))/RScale;
	}
	*/
	
	//terms for the RC
	/*  The equation that is implemented is 
	              /        /      1              \ \
	OCV_RC = R_1 | 1 - exp| - --------  \delta t | | * f_1,k
	              \        \   R_1 C_1           / /
	*/  
	OCV += (int16_t)(((double)R1[cell-1])/RScale*
		(1.0-exp((-(double)(RScale*CScale))/
			(((double)R1[cell-1]*(double)C1[cell-1]))))*
			R1I[cell-1]);
	
	//lastTime[cell-1] = time;

	
	return OCV;
}

PT_THREAD(updateOCV(struct pt* pt))
{
	PT_BEGIN(pt);
	initEEPROM();
	while(true)
	{
		if(oneSecondPassed & timeOCVMask)
		{
			oneSecondPassed &= ~timeOCVMask;
			
			for(uint8_t cell = 1; cell<=NumberOfCells; cell++)
			{
				//update voltage on C1 in RC
				/*
				  The equation for the voltage on the cap is
				                    /      1              \
				  R1I=f_1,k+1= exp |  - -------- \delta t | f_1,k + i_cell
				                    \   R_1 C_1          /
						
				  Units are the same as stackCurrent, so basically 1 x CurrentTick=1A
				*/

				R1I[cell-1] = exp((-(double)(RScale*CScale))/((double)(R1[cell-1]*C1[cell-1])))*
						R1I[cell-1]+
						(double)stackCurrent;
				
				if(isSet(cell-1,DischargerStatus))
				{
					R1I[cell-1] -= (double)(BalancerCurrent*CurrentTick);
				}

			}
		}
		PT_YIELD(pt);
	}

	PT_END(pt);
}

//using the cell voltage, get the corresponding SOC.  We use a 32 entry table,
//then interpolate to get the SOC
//uint8_max = 100% soc
//0 = 0% soc
uint8_t getSOC(int16_t CV)
{
	#define voltageTableLength 32
	static uint16_t voltageTable [voltageTableLength] PROGMEM={ 1759, 1946, 2023, 2043, 2056, 2067, 2082, 2088, 
																1946, 2043, 2067, 2088, 2097, 2108, 2120, 2137, 
																2023, 2067, 2091, 2108, 2128, 2159, 2200, 2252, 
																2043, 2088, 2108, 2137, 2185, 2252, 2334, 2481 };
	
	//first check bounds.  Return the limits if we are out of bounds.
	if(CV < pgm_read_word(&voltageTable[0]))
	{
		return 0;
	}
	
	if(CV > pgm_read_word(&voltageTable[voltageTableLength-1]))
	{
		return UINT8_MAX;
	}
	
	uint8_t idx = 0;
	for(; idx < voltageTableLength; idx++)
	{
		if(CV >= pgm_read_word(&voltageTable[idx]))
			break;
	}
	
	if (CV == pgm_read_word(&voltageTable[idx]))
		return ((uint16_t)(UINT8_MAX)*(uint16_t)idx)/voltageTableLength;
		
	uint16_t SOC;
	
	//can be uint since CV and n is guaranteed to be > nm1 (n minus 1)
	uint16_t nm1 = pgm_read_word(&voltageTable[idx-1]);
	uint16_t n = pgm_read_word (&voltageTable[idx]);
	
	//linear interpolation
	SOC = ((uint16_t)(idx - 1)*(uint16_t)UINT8_MAX)/voltageTableLength + 
		(UINT8_MAX/voltageTableLength*(uint16_t)(CV - nm1))/(n - nm1);
	
	return SOC;
}

bool cellsBalanced(CVRegPacket6802* voltagePacket, uint8_t Tolerance)
{
	int16_t minCell = INT16_MAX;
	int16_t maxCell = 0;
	int16_t currentCell;
	
	for(uint8_t i = 1; i <= NumberOfCells; i++)
	{
		//currentCell = getOCV(voltagePacket, i);
		currentCell = voltageFromCVReg(voltagePacket->reg.payload[0].bytes, i);
		if(currentCell < minCell)
			minCell = currentCell;
		
		if(currentCell > maxCell)
			maxCell = currentCell;
	}
	
	if((maxCell - minCell) > Tolerance)
		return false;
	else
		return true;
}

/*	Call this function to update the discharge multipliers.  It only needs to be called after updating the capacity measurements.
 *	
 */	
void calcIBDCHMult()
{
	uint32_t minQp = UINT32_MAX;
	
	//the equation we implement here works like this:
	//we want to find the "average cell capacity" given that we cannot
	//more charge from one cell to another will 100% efficiency
	//as it turns out we need to know where the dividing point between high
	//cells and low cells is, but we don't what capacity this occurs at (that's
	//our equation).  So, to find, it, we iterate over all possible dividing points
	//and pick the dividing point that minimizes Qp (our average capacity).
	//The equation for Qp (assuming cell capacities sorted from index 0 = low, to high) is
	//       _  N           _  i
	//     E \	      Q_n + \      Q_n
	//       /_ n=i+1       /_ n=1
	//Qp = ----------------------------
	//            En + i - Ei

	uint16_t sortCapacities [NumberOfCells];
	memcpy(sortCapacities, capacities, NumberOfCells);
	sortUint16(sortCapacities, NumberOfCells);


	uint32_t QpSub;
	uint32_t QpAdd;
	uint32_t Qp;
	
	//iterate through the possible values for i
	for(int i = 1; i <= NumberOfCells; i++)
	{
		QpSub=0;
		QpAdd=0;
		//implement the equation for this i value
		for(int j = i+1-1; j < NumberOfCells; j++)
		{
			QpSub += capacities[j];
		}
		for(int j = 0; j < i-1; j++)
		{
			QpAdd += capacities[j];
		}
		Qp = ((QpSub*BalanceEff)/100+QpAdd)/((NumberOfCells*BalanceEff)/100 + i - (i*BalanceEff)/100);

		if(Qp < minQp)
			minQp = Qp;
	}
	
	double I_BDCHMult = 0;
	int32_t QiSum;
	
	for(uint8_t i = 0; i< NumberOfCells; i++)
	{
		I_BDCHMult = (double)(1-(double)capacities[i])/((double)Qp);
						
		QiSum = 0;
		for (uint8_t j = 0; j < NumberOfCells; j++)
		{
			if(j != i)
			{
				QiSum += capacities[j];
			}
		}
		
		I_BDCHMult -= ((double)BalanceEff)/((double)NumberOfCells*(double)(BalanceEff)-(double)NumberOfCells)*QiSum;
						
		BDCDivide[i] = (int16_t)(I_BDCHMult*10000);
	}
	
}