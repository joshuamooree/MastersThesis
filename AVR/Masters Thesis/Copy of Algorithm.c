#include "Algorithm.h"
uint16_t EEMEM REEPROM [NumberOfCells];
uint16_t EEMEM R1EEPROM [NumberOfCells];
uint16_t EEMEM C1EEPROM [NumberOfCells];


int16_t getOCV(uint8_t cell, AlgorithmState* state, double* C1V);
void AlgorithmMainLoop (AlgorithmState* state)
{
	
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
	
	static uint16_t SOCCurve[64];
	static uint8_t remeberReturnState=0;
	//static uint8_t PostChargeState;
	static uint8_t ReturnState;
	static uint8_t maxCellIndex;
	static uint8_t minCellIndex;
	//static int16_t endI;
	static CVRegPacket6802 voltageRecord;
	static CVRegPacket6802 voltageRecordSettle;
	double C1V [NumberOfCells] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int16_t vtest = voltageFromCVReg(voltageRecord.bytes, 1);
	//voltageToCVReg(voltageRecord.bytes, 1, 0x5af);
	//vtest=voltageFromCVReg(voltageRecord.bytes, 1);
	//voltageToCVReg(voltageRecord.bytes, 1, 0x123);
	//vtest=voltageFromCVReg(voltageRecord.bytes, 1);

	if(*state ->NewADCReadings & PrintADC && state->mode != DoNothing)
	{
		//int16_t count;
		printf ("%5i\t", time);
		for(int i = 1; i <= NumberOfCells; i++)
		{
			//count = voltageFromCVReg(voltageRegisters.reg.payload[0].bytes, i);
					
			//print680xCV(count);
			if(state ->mode == ExtractParams )
			{
				print680xCV(voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, i));
			} else {
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
			*/
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
			break;
		case JustLogging:
			break;
		case BeginExtractParams:
			if(*state->time > 5)
			{
				state->state = PulseTest;
				ReturnState = AfterPulse;
			}				
			break;
		case BeginTopBalance:
			eeprom_read_block(state->R, REEPROM, NumberOfCells*sizeof(uint16_t));
			eeprom_read_block(state->R1, R1EEPROM, NumberOfCells*sizeof(uint16_t));
			eeprom_read_block(state->C1, C1EEPROM, NumberOfCells*sizeof(uint16_t));
			
			//state->state = Charge;
			state->state=CalibrateTopBalance;
			ReturnState = TopBalancing;
			break;
		case TopBalancing:
			state->state = CalibrateTopBalance;
			ReturnState = JustLogging;
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
		case PulseTest:
			//so, here's how we do our pulse test:
			//1) turn on the stack charger and set it to 1A
			//2) run that for 300 seconds
			//3) record cell voltages
			//4) turn off charger, record time
			//5) after 5 seconds, measure cell voltage again.
			//   R = (V1 - V2)/(1A/51).  Units for that are LT6802Ticks/ISenseTicks.
			//6) Wait until cell voltage hasn't changed for 10s.  This time is ~4R_2 C.
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
								 voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, i))*100
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
										         voltageFromCVReg(state->voltagePacket->reg.payload[0].bytes, i))*100)
												 /state->endI -
											     state->R[i-1];
							
								state->C1[i-1] = ((uint16_t)((*state -> time) - PulseSettleTime - state->tStart)*(100/2))
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
				state->state = AlgorithmError;
			}
			break;		
		case Discharge:
			puts_P(PSTR("<al<ISET 0.5>>\n"));	//set discharge I to 0.5A
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
			
			if(remeberReturnState == None)
			{
				remeberReturnState = ReturnState;
			}				
			else
			{
				ReturnState = remeberReturnState;
				remeberReturnState = 0;
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
								remeberReturnState = None;
								puts_P(PSTR("**Calibrate Balance Done**"));
							} else {
								remeberReturnState = ReturnState;
								ReturnState = CalibrateBottomBalance;	//come back here when you're done
								state->state = Discharge;
							}								
						} else {
							if(chargingDone)
							{
								//BalanceStatus = Balanced;
								state->state = ReturnState;	//we're done!
								remeberReturnState = None;
								puts_P(PSTR("**Calibrate Balance Done**"));
							} else {
								remeberReturnState = ReturnState;
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
		case AlgorithmError:
		default:
			CalibrateLEDPort &= ~(1 << CalibrateLED);	//Make sure calibrate light is on
			ErrorLEDPort &= ~(1 << ErrorLED);			//Turn on Error LED
			puts_P(PSTR("**Algorithm Error**"));
			puts_P(PSTR("<ps<S>>\n"));
			puts_P(PSTR("<al<ISET 0>>\n"));
			puts_P(PSTR("<al<LOAD 0>>\n"));
			break;			
		}				
}

int16_t getOCV(uint8_t cell, AlgorithmState* state, double* C1V)
{
	CVRegPacket6802* voltages = state->voltagePacket;
	uint16_t* R1 = state->R;
	uint16_t* R2 =  state->R1;
	uint16_t* C = state->C1;
	int16_t stackI = *state->stackCurrent;
	uint16_t DischargerStatus = *state->DischargerStatus;
	uint16_t time = *state->time;
	
	static uint16_t lastTime [NumberOfCells];
	int16_t OCV;
	
	OCV = voltageFromCVReg(voltages->reg.payload[0].bytes, cell);
	OCV -= R1[cell-1]/100*(stackI-(((1 << (cell-1))&DischargerStatus)*BalancerCurrent*CurrentTick));
	
	//OCV += ((double)R2[cell-1]*(double)C[cell-1])*(double)(time-lastTime);
	OCV += (uint16_t)((double)R2[cell-1]*
		(1.0-exp(100.0/(((double)R2[cell-1]*(double)C[cell-1])*(double)(time-lastTime[cell-1]))))*
		C1V[cell-1]);
	//OCV += //(double)R2[cell-1]*
	//	(uint16_t)((exp(100.0f/(((double)R2[cell-1]*(double)C[cell-1])*(double)(time-lastTime)))));
		//(double)C1V[cell-1];
		
	//C1V[cell-1] = exp(100.0/((double)R2[cell-1]*(double)C[cell-1]))*(double)C1V[cell-1]+
	//			  (double)stackI-
	//			  (double)(((1<<(cell-1))&DischargerStatus)*BalancerCurrent*CurrentTick);
	lastTime[cell-1] = *state->time;
	if(OCV < 1000)
		printf("\n%d\n",
		R1[cell-1]/100*(stackI-(int16_t)(((1 << (cell-1))&DischargerStatus)*(BalancerCurrent*CurrentTick))));
		
	return OCV;
}