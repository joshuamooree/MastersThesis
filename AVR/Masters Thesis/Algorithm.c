#include "Algorithm.h"
#include <float.h>

uint16_t EEMEM REEPROM [NumberOfCells];
uint16_t EEMEM R1EEPROM [NumberOfCells];
uint16_t EEMEM C1EEPROM [NumberOfCells];
int32_t EEMEM capEEPROM[NumberOfCells];

int32_t BDCMult[NumberOfCells];	//denominator is 10,000
#define BDCDenom 10000
uint8_t ActiveBalanceMode = ActiveBalanceOff;

void initEEPROM()
{
		eeprom_read_block(R, REEPROM, NumberOfCells*sizeof(uint16_t));
		eeprom_read_block(R1, R1EEPROM, NumberOfCells*sizeof(uint16_t));
		eeprom_read_block(C1, C1EEPROM, NumberOfCells*sizeof(uint16_t));
		eeprom_read_block(capacities, capEEPROM, NumberOfCells*sizeof(int32_t));
}

enum calibrateStates
{
	CalibrateTopBalance,
	CalibrateBottomBalance,
	BalanceDone
};

enum balanceStates
{
	PickCells,
	BalancerWait,
	SettleWait,
};


enum returnCodes
{
	allDone = 1,
	stillWorking = 0,
	needToCharge = 2,
};
uint8_t calibrateBalance(threadVar* var, uint8_t mode, uint8_t tolerance)
{
	//the algorithm looks like this
	//loop through the cells, looks for the cells that are
	//		greater than the average
	//		greater than min cell + tolerance
	//		not depleted
	//		not the min cell
	//if there are cells that meet the criteria,
	//		for the cells that meet the criteria, turn on the balancer for 30 seconds
	//		once 30 seconds pass, turn the balancers off
	//		wait 30 seconds for cells to rest
	//		repeat
	//else
	//		done
	
	static uint8_t state = PickCells;
	static uint32_t timeRec;
	static int16_t minDelta = INT16_MAX;
	static uint8_t onTime = 30;
	int32_t average = 0;
	int16_t cellV;
	int16_t minCellV = INT16_MAX;
	int16_t maxCellV = INT16_MIN;
	uint8_t minCell;
	
	if((NewADCReadings & ADCBalanceFlag) &&
			oneSecondPassed & secondAlgorithmMask)
	{
		NewADCReadings &= ~ADCBalanceFlag;
		oneSecondPassed &= ~secondAlgorithmMask;
		
		switch(state)
		{
			case PickCells:
				//first the average
				for(uint8_t i = 1; i <= NumberOfCells; i++)
				{
					cellV = voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i);
					average += cellV;
					
					if(cellV < minCellV)
					{
						minCellV = cellV;
						minCell = i;
					}
					if (cellV > maxCellV)
					{
						maxCellV = cellV;
					}
				}
				
				if((maxCellV - minCellV) < minDelta)
				{
					minDelta = maxCellV - minCellV;
				}
				
				#define upperLimit ((uint16_t)(0.20*cellOV*LT6802CountValue))
				#define lowerLimit ((uint16_t)(0.05*cellOV*LT6802CountValue))
				if(minDelta > upperLimit)
				{
					onTime = 30;
				} else if (minDelta < lowerLimit)
				{
					onTime = 10;
				} else {
					onTime = 20;
				//	//we want a linear scaling so that at upper limit, time=30 sec
				//	//and at lower limit, time = 10 sec
				//	onTime = ((30-10)*(minDelta - lowerLimit))/(upperLimit - lowerLimit)+10;
				}
				
				average *= 100;	//increase average by 100 so resolution is not lost during divide
				average /= NumberOfCells;
				
				DischargerStatus = 0x0000;

				if(minCellV <= voltageFromSOC(FullCharge/4))
				//(int16_t)(cellUV/LT6802CountValue))
				{
					
					return needToCharge;
				}
				
				//printf_P(PSTR("**%"PRIx32"**\n"), average);
				for (uint8_t i = 1; i <= NumberOfCells; i++)
				{
					if(i != minCell)
					{
						cellV = voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i);
						if ( ((cellV*100) > average) && 
						     ((cellV - minCellV) > tolerance) && 
							 (cellV > voltageFromSOC(FullCharge/4)) )
							 //(cellV > (int16_t)(1.1*cellUV/LT6802CountValue)) )
						{
								DischargerStatus |= (uint16_t)(1 << (i - 1));
						}
					}
				}
				
				
				if(DischargerStatus == 0x00)
				{
					//now make sure every cell is in tolerance
					for (uint8_t i = 1; i <= NumberOfCells; i++)
					{
						if(i != minCell)
						{
							cellV = voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i);
							if ( ((cellV - minCellV) > tolerance) && 
								//(cellV > (int16_t)(1.1*cellUV/LT6802CountValue)) 
								  (cellV > voltageFromSOC(FullCharge/4)) )
							{
								DischargerStatus |= (uint16_t)(1 << (i - 1));
							}
						}
					}
				}
				
				
				updateCFGReg = true;
				
				if(DischargerStatus == 0x00)
				{
					//yay, done!			
					return allDone;
				} else {
					timeRec = time;
					state = BalancerWait;
				}	
				
				break;
			case BalancerWait:
				if((time - timeRec) > onTime)
				{
					timeRec = time;
					DischargerStatus = 0x0000;
					state = SettleWait;
				}
				break;
			case SettleWait:
				if((time - timeRec) > 30)
				{
					state = PickCells;
				}
				break;
		}

	}
	
	return stillWorking;
}


PT_THREAD(calibrateTopBalance(threadVar* var, uint8_t SOC))
{
	PT_BEGIN(&var->ptVar);
	static threadVar subThread;
	
	
	subThread.state=var->state;
	
	balanceMode = normalDisch;
	BalanceLEDPort &= ~(1 << BalanceLED);
	uint8_t returnCode;
	
	while(true)
	{
		//the logic works like this
		//if we are done balancing and done charging, we are done
		//if we are done balancing, but not charging, we need to charge
		//if we are not done balancing, we need to repeat until we are
		returnCode = calibrateBalance(var, CalibrateTopBalance, (uint8_t)((cellOV/LT6802CountValue/InitBalanceTolerance)));
		if(returnCode == allDone)
		{
			puts_P(PSTR("**Possible balance**\n"));
			PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 10));
			if(cellsBalanced(var->state->voltagePacket, (uint8_t)((cellOV/LT6802CountValue/InitBalanceTolerance)*1.3)))
			{
				if(!doneCharging(var->state->voltagePacket, SOC))
				{
					PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1"), SOC));
				}
			
				BalanceLEDPort |= (1 << BalanceLED);
				puts_P(PSTR("**Calibrate Balance Done**\n"));
				//toleranceMult = 8;
				PT_EXIT(&var->ptVar);
			} else {
				//toleranceMult--;
			}
		} else if(returnCode == needToCharge)
		{
			PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1"), SOC));
		}
		//PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 1));
		
		PT_YIELD(&var->ptVar);
	}
	//DischargerStatus = 0x0000;
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

#define NewStylePulse
#ifndef NewStylePulse
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
					//memcpy(voltageRecord.bytes, var->state->voltagePacket->bytes, sizeof(CVRegPacket6802));
					for (uint8_t byte = 0; byte<sizeof(CVRegPacket6802); byte++)
					{
						voltageRecord.bytes[byte]=var->state->voltagePacket->bytes[byte];
					}
					puts_P(PSTR("<ps<S>>\n"));
					tStart = time;
					NewADCReadings &= ~ADCAlgorithm;
					state = PulseBeginSettle;
					force6802Conversion;
				}
				break;
			case PulseBeginSettle:
				if(NewADCReadings & ADCAlgorithm)
				{				
					for(int i = 1; i <= NumberOfCells; i++)
					{
						//R=(V_beforeStep-V_endStep)*RScale/I_step
						R[i-1]=(((uint32_t)(voltageFromCVReg(voltageRecord.reg.payload[0].bytes, i) - 
									 voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i)))
									 *RScale)
									 /endI;
									 
						C1[i-1] = UINT16_MAX;
						R1[i-1] = 0;
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
						
					tStart = time;
					state = PulseEndSettle;
				}
				break;
			case PulseEndSettle:
				if(NewADCReadings & ADCAlgorithm)
				{
					NewADCReadings &= ~ADCAlgorithm;
					// *state -> oneSecondPassed &= ~timeAlgorithmMask;
					
					bool allCellsSettled = true;
				
					enum{
						settlePre,
						settleWait,
						settleCalc,
						settleDone
					};
					
					uint8_t settleState = settlePre;
					
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
						
						//if C1 == UINT16_MAX, the cell isn't done yet.  Otherwise, we need
						//to figure out which state we should be in.
						if(C1[i-1] == UINT16_MAX)
						{
							if( abs(voltageFromCVReg(voltageRecordSettle.reg.payload[0].bytes, i) - 
								voltageFromCVReg(var->state ->voltagePacket->reg.payload[0].bytes, i)) 
								>= PulseSettleThreshold )
							{
								settleState = settlePre;
							} else if(R1[i-1] < PulseSettleTime) {
								settleState = settleWait;
							} else {
								settleState = settleCalc;
							}
							allCellsSettled = false;
						} else {
							settleState = settleDone;
						}
						
						switch(settleState)
						{
							case settlePre:
								voltageToCVReg(voltageRecordSettle.reg.payload[0].bytes, i, 
									voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i));

								R1[i-1] = 0;
								
								break;
							case settleWait:
								R1[i-1]++;
								break;
							case settleCalc:
								//R1=(V_beforeStep - V_settle)*RScale/I_pulse - R
								R1[i-1] =(((uint32_t)voltageFromCVReg(voltageRecord.reg.payload[0].bytes, i)-
												 voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i))*
												 RScale)
												 /endI -
												 R[i-1];

								//tau = RC, settle in ~2RC, T=2RC
								//C=(T_current - T_setlleBegin)*CScale/(2*R1/RScale)
								C1[i-1] = ((uint16_t)(time - tStart)*(CScale*RScale/2))
												/ R1[i-1];
								printf_P(PSTR("**%"PRId8" done %"PRId32"**\n"), i,((uint32_t)voltageFromCVReg(voltageRecord.reg.payload[0].bytes, i)-
												 voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i)));
								break;
							case settleDone:	//nothing to do
								break;
								
						}										
							/*			
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
						*/		
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
#else
#define OnTimePerCell 5
#define TimeBetweenCells 30
#define repetitions 5
PT_THREAD(pulseTest(threadVar* var))
{
	static uint32_t tBegin;
	static uint8_t repetitionCount=0;
	static uint8_t currentCell=0;
	PT_BEGIN(&var->ptVar);
	tBegin = time;
	
	while(repetitionCount < repetitions)
	{
		currentCell = 0;
		
		while(currentCell < NumberOfCells)
		{
			tBegin = time;
			DischargerStatus = (1 << currentCell);
			updateCFGReg = true;
			printf_P(PSTR("**Cell %i**\n"), currentCell);
			PT_WAIT_UNTIL(&var->ptVar, time >= tBegin + OnTimePerCell);
			DischargerStatus = 0x00;
			updateCFGReg = true;
			tBegin = time;
			PT_WAIT_UNTIL(&var->ptVar, time >= tBegin + TimeBetweenCells);
			
			currentCell++;
		}
		
		repetitionCount++;
	}
	
	PT_END(&var->ptVar);
}	
#endif

enum chargeStates
{
	Charge,
	CCCharge,
	CVCharge	
};

PT_THREAD(charge(threadVar* var, const char* current, uint8_t SOC))
{
	//static uint32_t timeBegin;
	PT_BEGIN(&var->ptVar);

	static uint8_t state;
	state = Charge;
	//timeBegin = time;
	
	while(true)
	{
		switch(state)
		{
			case Charge:
				DischargerStatus = 0x0000;
				updateCFGReg = true;
				
				//get a good ADC reading, then just exit if cells aren't charged
				NewADCReadings &= ~ADCAlgorithm;
				
				PT_WAIT_UNTIL(&var->ptVar, (NewADCReadings & ADCAlgorithm));
				
				if(doneCharging(var->state->voltagePacket, SOC))
				{
					puts_P(PSTR("**No charge necessary.**\n"));
					PT_EXIT(&var->ptVar);
				}					
				
				printf_P(PSTR("<ps<P55VC%SARG>>\n"), current);	//set the power supply to the stack V and current
														//to 0.5A
				
				puts_P(PSTR("**Begin CC Charge**\n"));
				state = CCCharge;
				
				break;
			case CCCharge:
				if(NewADCReadings & ADCAlgorithm)
				{
					NewADCReadings &= ~ADCAlgorithm;
					
					//enforce a minimum time for cc charge
					if(doneCharging(var->state->voltagePacket, SOC))
					{
						printPowerSupplyStackVString(var->state->voltagePacket, NumberOfCells, "C0.5A");
						state = CVCharge;
						puts_P(PSTR("**Begin CV Charge**\n"));
					} else {
						//just stay in this state - we're fine
					}
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

PT_THREAD(discharge(threadVar* var, const char* rate, uint8_t SOC))
{
	
	static uint8_t state;
	static uint8_t count;
	
	PT_BEGIN(&var->ptVar);
	
	state = Discharge;
	
	while(true)
	{
		switch(state)
		{
			case Discharge:
				if(doneDischarging(var->state->voltagePacket, SOC))
				{
					PT_EXIT(&var->ptVar);
				}	
				
				//do NOT allow the active load to discharge cells if we don't check the UV
				#ifndef AllowChargeOverDischargedCells  //disallow discharging when all we want is charge
				printf_P(PSTR("<al<ISET %S>>\n"), rate);	//set discharge I
				puts_P(PSTR("<al<LOAD 1>>\n"));		//connect load
				puts_P(PSTR("**Begin Discharge**\n"));
				state = DischargeMonitor;
				count = 0;
				#else
				state=Discharge;
				PT_EXIT(&var->ptVar);
				#endif
				
				break;
			case DischargeMonitor:
				if(NewADCReadings & ADCAlgorithm)
				{
					if(doneDischarging(var->state->voltagePacket, SOC))
					{
						if (count >= 2)
						{
							puts_P(PSTR("<al<ISET 0>>\n"));	//stop discharging
							puts_P(PSTR("<al<LOAD 0>>\n"));	//disconnect load
							state = Discharge;
							PT_EXIT(&var->ptVar);
						} else {
							count++;
						}

					} else {
						//just stay here
						count = 0; //reset for erroneous readings
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
	static uint32_t tBegin = 0;
	tBegin = time;
	
	PT_WAIT_WHILE(&var->ptVar, (time < (tBegin + delayS)));
	PT_EXIT(&var->ptVar);
	
	PT_END(&var->ptVar);
}

PT_THREAD(masterThread(threadVar* var))
{
	static threadVar subThread;
	//static uint16_t Qp;
	static uint32_t tempCap;				
	static uint8_t SOC;
	static uint32_t timeRec;
	static uint8_t i;
	//static uint8_t temp;
	
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
				PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge(&subThread, PSTR("0.1"), FullDischarge));
				shutdown();
				var->state->mode = Done;
				break;
			case ExtractParams:
				logMode = logRaw | logStackI | logBalancer;
				PT_WAIT_WHILE(&var->ptVar, time  <= 5);
				puts_P(PSTR("**Begin Pulse Test**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, pulseTest(&subThread));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, calibrateTopBalance(&subThread));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge(&subThread));
				puts_P(PSTR("**Pulse test done**\n"));
				var->state->mode = DoNothing;
				shutdown();
				break;
		
			case DoTopBalance:
				initEEPROM();
				logMode = logRaw | logStackI | logBalancer;
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 2));
				puts_P(PSTR("**Begin top balance initial charge**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.2"), FullCharge));
				puts_P(PSTR("**Begin top balance**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, calibrateTopBalance(&subThread, FullCharge));
				puts_P(PSTR("**Begin final charge**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.2"), FullCharge));
				var->state->mode = DoNothing;
				shutdown();
			
				break;
			case ChargeCells:
				puts_P(PSTR("**Begin charge**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1"), FullCharge));
				puts_P(PSTR("**End Charge**\n"));
				var->state->mode = DoNothing;
				shutdown();
				break;
			case Done:
				break;
			case FindCapacities:
				//logMode = logRaw;
				logMode = logRaw | logBalancer | logStackI;

				
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 30));
				
				
				PT_WAIT_UNTIL(&var->ptVar, NewISenseReadings & ISenseAlgorithm);
				
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.5"), FullCharge));
				
				puts_P(PSTR("**Begin Find Capacity**\n"));
				/*
				PT_SPAWN(&var->ptVar, &subThread.ptVar, calibrateTopBalance(&subThread, FullCharge));
				
				while(true)
				//while(false)
				{
					if(NewADCReadings & ADCAlgorithm)
					{
						NewADCReadings &= ~ADCAlgorithm;
						
						while(!cellsBalanced(var->state->voltagePacket, (uint8_t)((cellOV/LT6802CountValue/InitBalanceTolerance)*1.3)))
						{
							PT_SPAWN(&var->ptVar, &subThread.ptVar, calibrateTopBalance(&subThread, FullCharge));
							PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 30));
						}
						//one way or the other - if we got here, the pack's balanced
						break;
						

					}
					PT_YIELD(&var->ptVar);
				}
				*/
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 60));
				
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1"), FullCharge));

				//ok, we know the cells are definitely balanced.  Now we need to reset the coulomb counters
				//and discharge
				for(uint8_t i = 0; i < NumberOfCells; i++)
				{
					currentAccumulators[i] = 0;
				}
				
				DischargerStatus = 0x00;
				updateCFGReg = true;
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 60));
				
				PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge(&subThread, PSTR("0.1"), FullDischarge));
				//ok, now we have the cells discharged and we know minimum capacity.  Rest, then
				// use our table to figure out SOC.  From there, we estimate individual capacity.
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 60));
				

				puts_P(PSTR("**"));
				for(uint8_t i = 0; i < NumberOfCells; i++)
				{
					//if SOC=0, capacity should be simply the current accumulator
					//if SOC=0.25, smallest cell implied to be 75% of biggest cell, so capcity is current accumulator*1.33
					//if SOC=0.5, capacity should be current accumulator*2
					//if SOC=0.75, this implies cell is 4x bigger than smallest cell, so, capacity is current accumulator*4
					//if SOC=1, capacity should be infinite
					//so, 
					//   capacity = IAccumulator/(1-SOC)
					//	but get SOC is is scaled so 100% = UINT8_MAX, so multiply everything else by UINT8_MAX

					tempCap = (uint32_t)(-currentAccumulators[i])*UINT8_MAX;
					SOC = getSOC(voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i+1));
					capacities[i] = (int32_t)(tempCap/(UINT8_MAX-
						SOC));
					printf_P(PSTR("%"PRIx8"\t"), SOC);
				}
				puts_P(PSTR("**\n"));
				
				PT_YIELD(&var->ptVar);
				//now store in EEPROM
				eeprom_update_block(capacities, capEEPROM, NumberOfCells*sizeof(int32_t));
				
				//charge the pack so we don't leave it in a vulnerable place
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.5"), FullCharge));
				
				//finally, just do nothing
				puts_P(PSTR("**Done find capacity**\n"));

				shutdown();
				var->state->mode=DoNothing;

				break;
			case ActiveBalanceChargeDischarge:
				initEEPROM();
				
				ActiveBalanceMode = ActiveBalanceOff;
				//ActiveBalanceMode = ActiveBalanceBegin;
				//PT_YIELD(&var->ptVar);
				//PT_YIELD(&var->ptVar);
				
				puts_P(PSTR("**Begin Active balance**\n"));
				
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1"), FullCharge));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, calibrateTopBalance(&subThread, FullCharge));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 120));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge((&subThread), PSTR("0.1"), 0.9*FullCharge));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1"), FullCharge/2));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, calibrateTopBalance(&subThread, 0.9*FullCharge));
				puts_P(PSTR("**Done setup.  Beginning active balance Test**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 60*1));
				//ActiveBalanceMode = ActiveBalanceBegin;
				puts_P(PSTR("**Balancers engaged**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 60*1));
				//PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1"), FullCharge*0.9));
				//puts_P(PSTR("**Done charge test**\n"));
				//shutdown();//temporary
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 60*10));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge((&subThread), PSTR("0.1"), FullCharge*0.1));
				puts_P(PSTR("**Done discharge**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 60*10));
				puts_P(PSTR("**Begin Charge**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread, PSTR("0.1"), FullCharge*0.9));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, delay(&subThread, 60*10));
				
				ActiveBalanceMode = ActiveBalanceEnd;
				puts_P(PSTR("**Done Active Balance\n"));
				
				shutdown();
				break;
			case Test:	
				logMode = logRaw | logStackI | logBalancer;

				uint8_t a;
				a=voltageFromSOC(FullCharge/2);
				
				while(true)
				{
					for (i = 0; i < NumberOfCells; i++)
					{
						DischargerStatus = (1 << i);
						timeRec = time;
						PT_WAIT_UNTIL(&var->ptVar, (time - timeRec) > 30);
					}
				}
				
			default:
				globalError = AlgorithmErrorDefaultState;
			case AlgorithmError:
				#ifndef IgnoreErrors
				emergencyShutdown();
			
				CalibrateLEDPort &= ~(1 << CalibrateLED);	//Make sure calibrate light is on
				ErrorLEDPort &= ~(1 << ErrorLED);			//Turn on Error LED

				printf_P(PSTR("**ADCAlgorithm Error: %d**\n"), globalError);
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
	static double netStackI;
	
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
				logMode &= ~logPWMThresh;
				break;
			case ActiveBalanceBegin:
				calcIBDCHMult();	//calculates the constants
				puts_P(PSTR("**Multiplier constants: "));
				for(uint8_t i = 0; i < NumberOfCells; i++)
				{
					printf_P(PSTR("%"PRId32" "), BDCMult[i]);
				}
				puts_P(PSTR("**\n"));
				
				ActiveBalanceMode = ActiveBalanceOn;
				break;
			case ActiveBalanceOn:
				balanceMode = PWMDisch;
				maxResult = DBL_MIN;
				prevMaxResult = DBL_MAX;
				logMode |= logPWMThresh;
				logMode |= logStackI;
				
				PT_WAIT_UNTIL(pt, NewISenseReadings & ISenseAlgoMonitor);
				NewISenseReadings &= ~ISenseAlgoMonitor;
				
				//compensation for the fact that the measured stack current includes the output of the balancers
				#ifdef stackIIncludesBal
				balancerStackI = 0;
				for(uint8_t i=0; i < NumberOfCells; i++)
				{
					balancerStackI += balancePWMThresholds[i];
				}
				balancerStackI *= (BalancerCurrent/UINT8_MAX/CurrentTick);
				
				netStackI = ((double)(stackCurrent-balancerStackI))/CurrentTick;
				#else
				netStackI = ((double)stackCurrent)/CurrentTick;
				#endif
				maxResult = -DBL_MAX;
				//first find the largest discharge current
				#define fpTolerance 0.001
				
				for(uint8_t i = 0; i < NumberOfCells; i++)
				{
					//the equation here works like this:
					//First, we find the desired discharge current,
					//next, we find our what percentage of our discharge current that is
					//then scale so uint8_max = 100%
					//result = ((((double)ChargeI)*10000/BDCDivide[i])/BalancerCurrent)*UINT8_MAX;
					result = ((double)(netStackI * BDCMult[i]))/BDCMult[i];

					
					
					if(result > maxResult)
						maxResult = result;
				}
				
				//maxResult = DBL_MIN;
				prevMaxResult = DBL_MAX;
				
				//this is solving an iterative equation as described in chapter 2 of the thesis.  Basically, we need
				//to iterate until our solution for the balancer current stops changing

				//so, what we are implementing here looks like
				//            |-          /       \      -|
				// I        = |  I - max | I      | (1-E) |* I_BDCH
				//   Bin,n    |-  s      \  Bin,n /      -|
				//
				// In the below code, result is I_bin,n and maxResult will be, once the code is done, max(I_bin,n)
				//also note that here, unlike in other parts of the code, I is in A, not ADC ticks
				while( fabs(prevMaxResult - maxResult) > fpTolerance )
				{
					prevMaxResult = maxResult;
					tempMaxResult = -DBL_MAX;
					for(uint8_t i = 0; i < NumberOfCells; i++)
					{
						result = ((netStackI - (maxResult*(100-BalanceEff)/100.0))*BDCMult[i])/BDCDenom;
					
						if(result > tempMaxResult)
							tempMaxResult = result;
					}
					maxResult = tempMaxResult;
					//PT_YIELD(pt);
				}
				
				//so, now we know max(I_Bin,n) (its what maxResult holds), we can calculate the discharger current as
				//                           /          \           _
				// I         = I       - max | I        |           _
				//  BDCH,n       Bin,n       \   Bin,n  /           _
				//
				// in the end, all we need is to calculate the current as a percentage of our balancers' maximum current
				// code elsewhere will take care of turning the balancers on and off as needed to achieve the necessary
				// fraction
				//
				// in the below code, result is I_Bin,n
				//also note that here, unlike other parts of the code, current is in A, not ADC ticks
				for(uint8_t i = 0; i < NumberOfCells; i++)
				{
					result = ((netStackI - (maxResult*(100-BalanceEff)/100.0))*BDCMult[i])/BDCDenom;
					//don't let balancers try to do positive current (or negative duty cycle)
					if(result > maxResult)
					{
						balancePWMThresholds[i] = 0;
					} else {
						balancePWMThresholds[i] = (uint8_t)(((double)(-result+maxResult)/BalancerCurrent)*UINT8_MAX);
					}
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
	//OCV -= (((int32_t)R[cell-1])*stackCurrent)/RScale;
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
		if(oneSecondPassed & secondOCVMask && NewISenseReadings & ISenseUpdateOCV)
		{
			oneSecondPassed &= ~secondOCVMask;
			NewISenseReadings &= ~ISenseUpdateOCV;
			
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

	int32_t sortCapacities [NumberOfCells];
	memcpy(sortCapacities, capacities, NumberOfCells*sizeof(int32_t));
	sortInt32(sortCapacities, NumberOfCells);


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
	double I_BDCHPart = NumberOfCells;
	int32_t QiSum = 0;
	
	//we're implementing the formula
	//               |-           N    /        \      /        \  -|
	//               |       E   --   /     Q_i  \    /     Q_n  \  |
	// I_BIN,n = I_s |   ------- \   | 1 - ----  | - |  1 - ---  |  |
	//               |    NE - N /__ \      Q_p  /    \     Q_p  /  |
	//               |-          i=1  \         /      \        /  -|
	//I_BDCHMult is the part in the square brackets
	//I_BDCHPart is the sum.  Note that that part does not change per cell, so, we'll calculate it once.
	//Note that here, we rewrite the sum so that it becomes
	//           N  
	//      1  \--
	// N - ---  \   Q_i
	//     Q_p  /   
	//         /__
	//         i=1
	
	for (uint8_t j = 0; j < NumberOfCells; j++)
	{
		QiSum += capacities[j];
	}
	I_BDCHPart = QiSum;
	I_BDCHPart /= Qp;
			
	for(uint8_t i = 0; i< NumberOfCells; i++)
	{
		I_BDCHMult = -(double)(1-(double)capacities[i])/((double)Qp);
						
						
		I_BDCHMult += ((double)BalanceEff)/((double)NumberOfCells*(double)(BalanceEff)-(double)NumberOfCells)*I_BDCHPart;
						
		BDCMult[i] = (int32_t)(I_BDCHMult*BDCDenom);
	}
	
}
