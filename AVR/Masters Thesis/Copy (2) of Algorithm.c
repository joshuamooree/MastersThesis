#include "Algorithm.h"

uint16_t EEMEM REEPROM [NumberOfCells];
uint16_t EEMEM R1EEPROM [NumberOfCells];
uint16_t EEMEM C1EEPROM [NumberOfCells];






void initEEPROM(AlgorithmState* state)
{
		eeprom_read_block(R, REEPROM, NumberOfCells*sizeof(uint16_t));
		eeprom_read_block(R1, R1EEPROM, NumberOfCells*sizeof(uint16_t));
		eeprom_read_block(C1, C1EEPROM, NumberOfCells*sizeof(uint16_t));
}

enum calibrateStates
{
	CalibrateTopBalance,
	CalibrateBottomBalance,
	BalanceDone
};

bool calibrateBalance(threadVar* var, uint8_t* state)
{
	static threadVar subThread;
	static uint8_t maxCellIndex;
	static uint8_t minCellIndex;
	
	switch(*state)
	{
		case CalibrateTopBalance:		//these two states share much code
		case CalibrateBottomBalance:	
			
			//ok, we're done discharging, now we want to shuffle current around until
			//all the cells have the same voltage within ~5%
			if(NewADCReadings & Algorithm)
			{
				int16_t minCellV = INT16_MAX;
				int16_t maxCellV = 0;
				int16_t cellVoltage;
				
				//basically we aren't balancing
				if(DischargerStatus == 0)
				{
					maxCellIndex = 0;
					minCellIndex = 0;
				}						

				//if(voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, maxCellIndex+1) > 
				//   voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, minCellIndex+1))
				if(getOCV(maxCellIndex+1, var->state->voltagePacket, *var->state->stackCurrent) > 
					getOCV(minCellIndex+1, var->state->voltagePacket, *var->state->stackCurrent))
				{
					//do nothing  - just let it keep going
				} else {
					for(int i = 1; i <= NumberOfCells; i++)
					{
						//cellVoltage = voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, i);
						cellVoltage = getOCV(i, var->state->voltagePacket, *var->state->stackCurrent);
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
							
					if(minCellV < (maxCellV-maxCellV/InitBalanceTolerance))
					{
						DischargerStatus = (1 << maxCellIndex);
						updateCFGReg = true;
						
						//chargingDone = false;
						//dischargingDone = false;
					} else {	//we found no cells to balance - discharge some more
						DischargerStatus = 0;
						updateCFGReg = true;
					
						//the logic here is if the balance is complete, go to 
						//the state we do after the balance, otherwise,
						//charge/discharge
						if(*state == CalibrateBottomBalance)
						{
							if(doneDischarging(var->state->voltagePacket))
							{
								//BalanceStatus = Balanced;

								*state=BalanceDone;
								puts_P(PSTR("**Calibrate Balance Done**\n"));
							} else {
								//rememberReturnState = ReturnState;
								//ReturnState = CalibrateBottomBalance;	//come back here when you're done
								//var->state->state = Discharge;
								subThread.state=var->state;
								PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge(&subThread));
							}			
						} else {
							if(doneCharging(var->state->voltagePacket))
							{
								//BalanceStatus = Balanced;
								*state=BalanceDone;
								puts_P(PSTR("**Calibrate Balance Done**\n"));
							} else {
								//rememberReturnState = ReturnState;
								//ReturnState = CalibrateTopBalance;	//come back here when you're done
								//var->state->state = Charge;
								subThread.state=var->state;
								PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread));
							}
						}
					}
				}					
				NewADCReadings &= ~Algorithm;
			}
			break;
		case BalanceDone:
			BalanceLEDPort |= (1 << BalanceLED);
			return false;
			break;
	}
	return true;
}

PT_THREAD(calibrateTopBalance(threadVar* var))
{
	static uint8_t state;
	
	PT_BEGIN(&var->ptVar);
	
	state = CalibrateTopBalance;
	
	if(!calibrateBalance(var, &state))
		PT_EXIT(&var->ptVar);
	
	while(true)
	{
		do {
			
		}while()
	
		PT_YIELD(&var->ptVar);
	}		
	PT_END(&var->ptVar);
}

PT_THREAD(calibrateBottomBalance(threadVar* var))
{
	static uint8_t state;
	
	PT_BEGIN(&var->ptVar);
	
	 state = CalibrateBottomBalance;
	
	while(true)
	{	
		if(!calibrateBalance(var, &state))
			PT_EXIT(&var->ptVar);
	
		PT_YIELD(&var->ptVar);
	}	
	PT_END(&var->ptVar);
}

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
	static uint16_t tStart = 0;
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
				//   R = (V1 - V2)/(1A/51).  Units for that are LT6802Ticks/ISenseTicks.
				//6) Wait until cell voltage hasn't changed for 10s.  This time is ~4R_2 C1.
				//   R_2 = (V1 - V3)/(1A/51) - R_1
				//   C1 = (time - 10 - origTime)/4/R_2 units are seconds/(LT6802Ticks/ISenseTicks)
				puts_P(PSTR("<ps<P55VC1ARG>>\n"));
				//puts_P(PSTR("<ps<P55VC1ARG>>\n"));
				//printPowerSupplyStackVString(state->voltagePacket, NumberOfCells, "C0.5A");
				tStart = time;
				state = PulseTestEndPulse;
				break;
			case PulseTestEndPulse:
				if((time) > (tStart+PulseWidth))
				{
					endI = *var->state->stackCurrent;
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
									R1[i-1] =((voltageFromCVReg(voltageRecord.reg.payload[0].bytes, i)-
													 voltageFromCVReg(var->state->voltagePacket->reg.payload[0].bytes, i))*
													 RScale)
													 /endI -
													 R[i-1];

									C1[i-1] = ((uint16_t)(time - 
															PulseSettleTime - 
															tStart)*(RScale/2))
													/ R1[i-1];
								} else {
									R1[i-1]++;
									allCellsSettled = false;
								}
							} //else this cell is done
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

PT_THREAD(charge(threadVar* var))
{
	static uint8_t state = Charge;
	PT_BEGIN(&var->ptVar);
	
	while(true)
	{
		switch(state)
		{
			case Charge:
				CalibrateLEDPort &= ~(1 << CalibrateLED);	//notify the user we are calibrating
				puts_P(PSTR("<ps<P55VC0.5ARG>>\n"));		//set the power supply to the stack V and current
														//to 0.5A
				puts_P(PSTR("**Begin CC Charge**\n"));
				state = CCCharge;
				break;
			case CCCharge:
				DischargerStatus = 0x0000;
				updateCFGReg = true;
		
				if(NewADCReadings & Algorithm)
				{
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
				if(doneCharging(var->state->voltagePacket))
				{
					//the last part of the if statement prevents the CV from exiting too soon if we are close
					//to CV already
					if(*(var->state->stackCurrent) < minChargeI)
					{
						puts_P(PSTR("<ps<S>>\n"));
						state = Charge;
						PT_EXIT(&var->ptVar);
					} else {
						//stay here
					}
				}
				else
				{
					//globalError = AlgorithmErrorCVCharge;
					//PT_EXIT(&var->ptVar);
				}
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

PT_THREAD(discharge(threadVar* var))
{
	static uint8_t state = Discharge;
	PT_BEGIN(&var->ptVar);
	
	while(true)
	{
		switch(state)
		{
			case Discharge:
				puts_P(PSTR("<al<ISET 0.25>>\n"));	//set discharge I to 0.5A
				puts_P(PSTR("<al<LOAD 1>>\n"));		//connect load
				puts_P(PSTR("**Begin Discharge**\n"));
				//chargingDone = false;
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


PT_THREAD(masterThread(threadVar* var))
{
	static threadVar subThread;
	
	PT_BEGIN(&var->ptVar);
	subThread.state->voltagePacket = var->state->voltagePacket;
	subThread.state->stackCurrent = var->state->stackCurrent;
	
	while(true)
	{
		switch(var->state->mode)
		{
			case DoNothing:
				logMode = logNone;		
				break;
				
			case JustLog:
				initEEPROM(var->state);
				logMode = logRaw;
				
				puts_P(PSTR("**Begin just logging**\n"));
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
				initEEPROM(var->state);
				logMode = logRaw;
				
				puts_P(PSTR("**Begin initial charge for top balance**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, charge(&subThread));
				puts_P(PSTR("**Begin top balance**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, calibrateTopBalance(&subThread));
				puts_P(PSTR("**Begin discharge for top balance**\n"));
				PT_SPAWN(&var->ptVar, &subThread.ptVar, discharge(&subThread));
				var->state->mode = JustLog;
			
				break;
			case ChargeCells:
				puts_P(PSTR("Beginning charging"));
				break;
			case Done:
				break;
			default:
				globalError = AlgorithmErrorDefaultState;
			case AlgorithmError:
				emergencyShutdown();
			
				CalibrateLEDPort &= ~(1 << CalibrateLED);	//Make sure calibrate light is on
				ErrorLEDPort &= ~(1 << ErrorLED);			//Turn on Error LED

				printf_P(PSTR("**Algorithm Error: %d**\n"), globalError);
				break;	
		}
		PT_YIELD(&var->ptVar);
	}	
	
	PT_END(&var->ptVar);
}


/*
void AlgorithmMainLoop (AlgorithmState* state)
{
	static threadVar variables;
	variables.state = state;
	
	PT_INIT(&variables.ptVar);
	masterThread(&variables);
	
	
	bool chargingDone=doneCharging(state->voltagePacket);
	bool dischargingDone=doneDischarging(state->voltagePacket);
	static uint16_t dischargerTripPoint[NumberOfCells];
	///////////////////////////////////////////////
	// Variables related to balancing cells
	///////////////////////////////////////////////
	//the units are ADC ticks - seconds
	static uint16_t capacities[NumberOfCells] = {	5.0/20*UINT16_MAX,
													4.6/20*UINT16_MAX,
													5.2/20*UINT16_MAX,
													5.1/20*UINT16_MAX,
													4.9/20*UINT16_MAX,
													4.8/20*UINT16_MAX,
													4.7/20*UINT16_MAX,
													5.0/20*UINT16_MAX,
													5.1/20*UINT16_MAX,
													4.8/20*UINT16_MAX,
													4.7/20*UINT16_MAX,
													4.6/20*UINT16_MAX};
		
	static uint16_t Qp;
	static uint8_t errorCode = NoError;							
	
	static uint16_t SOCCurve[64];
	static uint8_t rememberReturnState=None;
	//static uint8_t PostChargeState;
	static uint8_t ReturnState;
	static uint8_t maxCellIndex;
	static uint8_t minCellIndex;
	//static int16_t endI;
	static CVRegPacket6802 voltageRecord;
	static CVRegPacket6802 voltageRecordSettle;
	double C1V [NumberOfCells] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	//int16_t vtest = voltageFromCVReg(voltageRecord.bytes, 1);
	//voltageToCVReg(voltageRecord.bytes, 1, 0x5af);
	//vtest=voltageFromCVReg(voltageRecord.bytes, 1);
	//voltageToCVReg(voltageRecord.bytes, 1, 0x123);
	//vtest=voltageFromCVReg(voltageRecord.bytes, 1);

	if(*state ->NewADCReadings & PrintADC && state->mode != DoNothing && state->state != BeginDoNothing)
	{
		//int16_t count;
		printf ("%5i\t", time);
		for(int i = 1; i <= NumberOfCells; i++)
		{
			//count = voltageFromCVReg(voltageRegisters.reg.payload[0].bytes, i);
					
			//print680xCV(count);
			if(state ->mode == ExtractParams || state->mode == Discharge)
			{
				print680xCV(voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, i));
			} else {
				/*
				if(i==2)
					printf("\n\n%d\n\n", getOCV(i, state, C1V));
				* /
				print680xCV(getOCV(i, state, C1V));
			}				
							
			
			
			//each count of current accumulator is worth
				//20 A / 1024 = 19.53 mA-s - well call it 20mA - s
				//we are reporting A - seconds and there are 50 20mA per A
				printf("\t%4ld\t", state->currentAccumulators[i-1]/51);
		}
		printf("\n");
		//we used the ADC readings, now we are done with them
			
			
		*state->NewADCReadings &= ~PrintADC;
	}
	
	//don't do anything for 5 seconds after starting
	if(*state->time < 5)
		return;
	
	//we are either calibrating or we are balancing
	switch(state->state)
	{
		case AlgorithmBegin:
		/*
			if(state->mode == DoNothing)
				state->state=BeginDoNothing;
			else if(state->mode == JustLog)
				state->state = BeginJustLog;
			else if(state->mode == ExtractParams)
				state->state=BeginExtractParams;
			else
				state ->state = AlgorithmError;
			* /
			switch(state->mode)
			{
				case DoNothing:
					state->state = BeginDoNothing;
					break;
				case JustLog:
					state->state = BeginJustLog;
					break;
				case ExtractParams:
					state->state = BeginExtractParams;
					break;
				case DoTopBalance:
					state->state = BeginTopBalance;
					break;
				default:
					errorCode = ErrorDefaultMode;
					state->state = AlgorithmError;
					break;
			}		
			break;
		case BeginDoNothing:
			break;
		case BeginJustLog:
			eeprom_read_block(state->R, REEPROM, NumberOfCells*sizeof(uint16_t));
			eeprom_read_block(state->R1, R1EEPROM, NumberOfCells*sizeof(uint16_t));
			eeprom_read_block(state->C1, C1EEPROM, NumberOfCells*sizeof(uint16_t));
			state->state=JustLogging;
			puts_P(PSTR("**Begin just logging**\n"));
			break;
		case JustLogging:
			break;
		case BeginExtractParams:
			if(*state->time > 5)
			{
				state->state = PulseTestBegin;
				ReturnState = AfterPulse;
			}
			puts_P(PSTR("**Begin parameter extraction**\n"));
			break;
		case BeginTopBalance:
			eeprom_read_block(state->R, REEPROM, NumberOfCells*sizeof(uint16_t));
			eeprom_read_block(state->R1, R1EEPROM, NumberOfCells*sizeof(uint16_t));
			eeprom_read_block(state->C1, C1EEPROM, NumberOfCells*sizeof(uint16_t));
			
			state->state = Charge;
			//state->state=CalibrateTopBalance;
			ReturnState = TopBalancing;
			puts_P(PSTR("**Begin initial charge for top balance**\n"));
			break;
		case TopBalancing:
			state->state = CalibrateTopBalance;
			ReturnState = TopBalancing2;
			puts_P(PSTR("**Begin top balance**\n"));
			break;
		case TopBalancing2:
			state->state = Discharge;
			ReturnState = JustLogging;
			puts_P(PSTR("**Begin discharge for top balance**\n"));
			break;
		case AfterPulse:
			state -> state = Charge;
			ReturnState = AfterCharge;
			break;
		case AfterCharge:
			state -> state = CalibrateTopBalance;
			ReturnState = AfterTopBalance;
			break;
		case AfterTopBalance:
			state -> state = Discharge;
			ReturnState = AfterDischarge;
			break;
		case AfterDischarge:
			state->state = BalanceDone;
			break;
		case PulseTestBegin:
			//so, here's how we do our pulse test:
			//1) turn on the stack charger and set it to 1A
			//2) run that for 300 seconds
			//3) record cell voltages
			//4) turn off charger, record time
			//5) after 5 seconds, measure cell voltage again.
			//   R = (V1 - V2)/(1A/51).  Units for that are LT6802Ticks/ISenseTicks.
			//6) Wait until cell voltage hasn't changed for 10s.  This time is ~4R_2 C1.
			//   R_2 = (V1 - V3)/(1A/51) - R_1
			//   C1 = (time - 10 - origTime)/4/R_2 units are seconds/(LT6802Ticks/ISenseTicks)
			puts_P(PSTR("<ps<P55VC1ARG>>\n"));
			//printPowerSupplyStackVString(state->voltagePacket, NumberOfCells, "C0.5A");
			state->tStart = *state ->time;
			state -> state = PulseTestEndPulse;
			break;
		case PulseTestEndPulse:
			if((*state -> time) > (state ->tStart+300))
			{
				state->endI = *state->stackCurrent;
				memcpy(voltageRecord.bytes, state->voltagePacket->bytes, sizeof(CVRegPacket6802));
				puts_P(PSTR("<ps<S>>\n"));
				state -> tStart = *state -> time;
				state -> state = PulseBeginSettle;
			}
			break;
		case PulseBeginSettle:
			if((*state->time) > (state->tStart + 1))
			{
				for(int i = 1; i <= NumberOfCells; i++)
				{
					state->R[i-1]=(voltageFromCVReg(voltageRecord.reg.payload[0].bytes, i) - 
								 voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, i))*RScale
								 /state->endI;
				}
				memcpy(voltageRecordSettle.bytes, state->voltagePacket->bytes, sizeof(CVRegPacket6802));
				state ->R1[0] = 0;
				*state -> NewADCReadings &= ~Algorithm;
				state -> tStart = *state -> time;
				state -> state = PulseEndSettle;
			}
			break;
		case PulseEndSettle:
			if(((*state -> NewADCReadings) & Algorithm) && 
			   //((*state ->oneSecondPassed) & timeAlgorithmMask) && 
			   (*state->time > state->tStart + 10))
			{
				*state -> NewADCReadings &= ~Algorithm;
				//*state -> oneSecondPassed &= ~timeAlgorithmMask;
					
				bool allCellsSettled = true;
				
				for	(int i = 1; i <= NumberOfCells; i++)
				{
					//printf("%i\n", abs(voltageFromCVReg(voltageRecordSettle.reg.payload[0].bytes, i) - 
					//    voltageFromCVReg(state ->voltagePacket->reg.payload[0].bytes, i)) );
					//if we've settled out (we've seen readings within ~9mV for 20s)
					if( abs(voltageFromCVReg(voltageRecordSettle.reg.payload[0].bytes, i) - 
					    voltageFromCVReg(state ->voltagePacket->reg.payload[0].bytes, i)) <= PulseSettleThreshold )
					{
						if(state ->C1[i-1] == UINT16_MAX)
						{
							if(state -> R1[i-1] >= PulseSettleTime)
							{
								state->R1[i-1] =((voltageFromCVReg(voltageRecord.reg.payload[0].bytes, i)-
										         voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, i))*RScale)
												 /state->endI -
											     state->R[i-1];
							
								state->C1[i-1] = ((uint16_t)((*state -> time) - PulseSettleTime - state->tStart)*(RScale/2))
												/ state->R1[i-1];
							} else {
								state->R1[i-1]++;
								allCellsSettled = false;
							}
						} //else this cell has already settled out
					} else {
						//memcpy(voltageRecordSettle.bytes, state->voltagePacket->bytes, sizeof(CVRegPacket6802));
						voltageToCVReg(voltageRecordSettle.reg.payload[0].bytes, i, 
						voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, i));
						
						state -> R1[i-1] = 0;
						state -> C1[i-1] = UINT16_MAX;
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
					

					state ->state = ReturnState;
					eeprom_update_block(state->R, REEPROM, NumberOfCells*sizeof(uint16_t));
					eeprom_update_block(state->R1, R1EEPROM, NumberOfCells*sizeof(uint16_t));
					eeprom_update_block(state->C1, C1EEPROM, NumberOfCells*sizeof(uint16_t));
					puts_P(PSTR("**Done with pulse test**\n"));
				}
			}
			break;
		case Charge:
			CalibrateLEDPort &= ~(1 << CalibrateLED);	//notify the user we are calibrating
			chargingDone = false;
			puts_P(PSTR("<ps<P55VC0.5ARG>>\n"));		//set the power supply to the stack V and current
														//to 0.5A
			puts_P(PSTR("**Begin CC Charge**\n"));
			state->state = CCCharge;
			break;
		case CCCharge:
			DischargerStatus = 0x0000;
			*state->updateCFGReg = true;
		
			if(*state->NewADCReadings & Algorithm)
			{
				if(chargingDone)
				{
					//int16_t volatageSum = CellVoltageSum(& voltageRegisters, NumberOfCells);
					//puts_P((PSTR("<ps<P")));		//the next three lines set the power supply to
					//print680xCV(volatageSum);		//the stack voltage and set the charge current
					//puts_P(PSTR("VC0.05ARG>>\n"));	//to 0.05A
					printPowerSupplyStackVString(state->voltagePacket, NumberOfCells, "C0.5A");
					state->state = CVCharge;
					puts_P(PSTR("**Begin CV Charge**\n"));
				} else {
					//just stay in this state - we're fine
				}
				*(state->NewADCReadings) &= ~Algorithm;
			}
			break;
		case CVCharge:
			if(chargingDone)
			{
				//the last part of the if statement prevents the CV from exiting too soon if we are close
				//to CV already
				if(*(state->stackCurrent) < minChargeI)
				{
					puts_P(PSTR("<ps<S>>\n"));
					state->state = ReturnState;
				} else {
					//stay here
				}
			}
			else
			{
				errorCode = CVCharge;
				state->state = AlgorithmError;
			}
			break;		
		case Discharge:
			puts_P(PSTR("<al<ISET 0.25>>\n"));	//set discharge I to 0.5A
			puts_P(PSTR("<al<LOAD 1>>\n"));		//connect load
			puts_P(PSTR("**Begin Discharge**\n"));
			chargingDone = false;
			state->state = DischargeMonitor;
			break;
		case DischargeMonitor:
			if(*state->NewADCReadings & Algorithm)
			{
				if(dischargingDone)
				{
					puts_P(PSTR("<al<ISET 0>>\n"));	//stop discharging
					puts_P(PSTR("<al<LOAD 0>>\n"));	//disconnect load
					state->state = ReturnState;
				} else {
					//just stay here
				}
			}
			break;
		case CalibrateTopBalance:		//these two states share much code
		case CalibrateBottomBalance:
			
			if(rememberReturnState == None)
			{
				rememberReturnState = ReturnState;
			}				
			else
			{
				ReturnState = rememberReturnState;
				rememberReturnState = None;
			}				
			
			//ok, we're done discharging, now we want to shuffle current around until
			//all the cells have the same voltage within ~5%
			if(*state->NewADCReadings & Algorithm)
			{
				int16_t minCellV = INT16_MAX;
				int16_t maxCellV = 0;
				int16_t cellVoltage;
				
				//basically we aren't balancing
				if(*state->DischargerStatus == 0)
				{
					maxCellIndex = 0;
					minCellIndex = 0;
				}						

				//if(voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, maxCellIndex+1) > 
				//   voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, minCellIndex+1))
				if(getOCV(maxCellIndex+1, state, C1V) > getOCV(minCellIndex+1, state, C1V))
				{
					//do nothing  - just let it keep going
				} else {
					for(int i = 1; i <= NumberOfCells; i++)
					{
						//cellVoltage = voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, i);
						cellVoltage = getOCV(i, state, C1V);
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
							
					if(minCellV < (maxCellV-maxCellV/InitBalanceTolerance))
					{
						*state->DischargerStatus = (1 << maxCellIndex);
						*state->updateCFGReg = true;
						
						chargingDone = false;
						dischargingDone = false;
					} else {	//we found no cells to balance - discharge some more
						DischargerStatus = 0;
						*state->updateCFGReg = true;
					
						//the logic here is if the balance is complete, go to 
						//the state we do after the balance, otherwise,
						//charge/discharge
						if(state->state == CalibrateBottomBalance)
						{
							if(dischargingDone)
							{
								//BalanceStatus = Balanced;
								state->state = ReturnState;	//we're done
								rememberReturnState = None;
								puts_P(PSTR("**Calibrate Balance Done**\n"));
							} else {
								rememberReturnState = ReturnState;
								ReturnState = CalibrateBottomBalance;	//come back here when you're done
								state->state = Discharge;
							}								
						} else {
							if(chargingDone)
							{
								//BalanceStatus = Balanced;
								state->state = ReturnState;	//we're done!
								rememberReturnState = None;
								puts_P(PSTR("**Calibrate Balance Done**\n"));
							} else {
								rememberReturnState = ReturnState;
								ReturnState = CalibrateTopBalance;	//come back here when you're done
								state->state = Charge;
							}
						}
					}
				}					
				*state->NewADCReadings &= ~Algorithm;
			}
			break;
		case BalanceDone:
			BalanceLEDPort |= (1 << BalanceLED);
			break;
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
		/*
		case Wait5Sec:
			if(state->tStart < state ->time - 5)
				state->tStart = state->time;
			else if(state->time > state->tStart + 5)
				state->state = ReturnState;
			break;
		case Wait20Sec:
			if(state->tStart < state ->time - 20)
				state->tStart = state->time;
			else if(state->time > state->tStart + 20)
				state->state = ReturnState;
		* /
		default:
			errorCode = AlgorithmErrorDefaultState;
		case AlgorithmError:
			emergencyShutdown(state->DischargerStatus);
			
			CalibrateLEDPort &= ~(1 << CalibrateLED);	//Make sure calibrate light is on
			ErrorLEDPort &= ~(1 << ErrorLED);			//Turn on Error LED
			//puts_P(PSTR("<ps<S>>\n"));
			//puts_P(PSTR("<al<ISET 0>>\n"));
			//puts_P(PSTR("<al<LOAD 0>>\n"));
			printf_P(PSTR("**Algorithm Error: %d**\n"), errorCode);
			state->state=BeginDoNothing;
			break;			
		}				
}
*/

int16_t getOCV(uint8_t cell, CVRegPacket6802* voltages, int16_t stackI)
{
	//CVRegPacket6802* voltages = state->voltagePacket;
	//int16_t stackI = *state->stackCurrent;
	
	static uint16_t lastTime [NumberOfCells];
	int16_t OCV;
	
	OCV = voltageFromCVReg(voltages->reg.payload[0].bytes, cell);
	
	//terms for the series R
	OCV += (int16_t)(R[cell-1]*stackI)/RScale;
	OCV -= (int16_t)(R[cell-1]*((int16_t)(isSet(cell-1, DischargerStatus)*BalancerCurrent*CurrentTick)))/RScale;
	
	//terms for the RC
	OCV += (int16_t)((double)R1[cell-1]*
		(1.0-exp(100.0/(((double)R1[cell-1]*(double)C1[cell-1])*(double)(time-lastTime[cell-1]))))*
		C1V[cell-1]);
	
	//update voltage on C1 in RC
	C1V[cell-1] = exp(100.0/((double)R1[cell-1]*(double)C1[cell-1]))*(double)C1V[cell-1]+
				  (double)stackI-
				  (double)(((1<<(cell-1))&DischargerStatus)*BalancerCurrent*CurrentTick);
	lastTime[cell-1] = time;
	
	/*
	if(OCV < 0)
	{
		printf("\n\n%d\t%d\n\n",
		voltageFromCVReg(voltages->reg.payload[0].bytes, cell)+
		R[cell-1]*((int16_t)(isSet(cell-1, DischargerStatus)*BalancerCurrent*CurrentTick))/RScale,
		OCV);
	}
	*/
	
	
		
	return OCV;
}