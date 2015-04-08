#include "Utils.h"

//using the cell voltage, get the corresponding SOC.  We use a 32 entry table,
//then interpolate to get the SOC
//uint8_max = 100% soc
//0 = 0% soc
uint8_t getSOC(int16_t CV)
{
	#define voltageTableLength 32
	const static int16_t voltageTable [voltageTableLength] PROGMEM={ 2000,2326,2411,2425,2433,2448,2460,2468,2474,2481,2486
									   2492,2498,2503,2510,2517,2526,2535,2546,2559,2573,2588,
									   2604,2620,2637,2655,2673,2692,2711,2731,2754,2800 };

	

	//first check bounds.  Return the limits if we are out of bounds.
	if(CV > pgm_read_word(&(voltageTable[voltageTableLength-1])))
	{
		return UINT8_MAX;
	}
	
	int16_t tableV = pgm_read_word(&(voltageTable[0]));
	
	if(CV <= tableV)
	{
		return 0;
	}
	
	uint8_t idx;
	
	for(idx = 1; (idx < (voltageTableLength - 1)) && (tableV < CV); idx++)
	{
		tableV = pgm_read_word(&(voltageTable[idx]));
	}
	
	
	if (CV == tableV)
		return ((uint16_t)(UINT8_MAX)*(uint16_t)(idx))/voltageTableLength;
		
	int16_t SOC;

	idx--;	//for loop incremented idx 1 time too many
	//can be uint since CV and n is guaranteed to be > nm1 (n minus 1)
	int16_t nm1 = pgm_read_word(&(voltageTable[idx-1]));
	int16_t n = pgm_read_word (&(voltageTable[idx]));
	
	//linear interpolation[
	//the index is the percentage (e.g. index = 0 = 0%, index = voltageTableLength-1 = 100%)
	//nm1 and n are the two data points that surround the voltage we measure
	//SOC = uint8_max = 100%
	//from 2 point form of line equation,
	//           y_2 - y_1
	//y = y_1 + ---------- (x - x_1)
	//           x_2 - x_1
	//here, x = CV, Y = SOC
	//      y_1 = (index - 1)*UINT8_MAX/(voltageTableLength-1)
	//		y_2 = (index)*UINT8_MAX/(voltageTableLength-1)
	//		x_1 = nm1
	//		x_2 = n
	//                                                    UINT8_MAX/(voltageTableLength-1)
	// SOC = (index-1)*UINT8_MAX/(voltageTableLength-1) + --------------------------------- * (CV - nm1)
	//                                                                 n - nm1
	//	or
	//       /                      (CV - nm1)*UINT8_MAX   \    /
	// SOC = | (index-1)*UINT8_MAX + --------------------  |   /  (voltageTableLength - 1)
	//       \                           (n - nm1)         /  /
	//SOC = ((uint16_t)(idx - 1)*(uint16_t)UINT8_MAX)/voltageTableLength + 
	//	(UINT8_MAX/voltageTableLength*(uint16_t)(CV - nm1))/(n - nm1);
	
	//SOC = (((uint16_t)(idx-1))*UINT8_MAX + (((uint16_t)(CV - nm1))*UINT8_MAX)/(n-nm1))
	//		/(voltageTableLength-1);
	SOC = (int16_t)(idx-1)*UINT8_MAX;
	SOC += (((int16_t)(CV - nm1))*UINT8_MAX)/((int16_t)(n-nm1));
	SOC /= voltageTableLength-1;
	
	return SOC;
}

/* SOC = 0 == 0% state of charge
 * SOC = UINT8_MAX == 100% state of charge
 */
int16_t voltageFromSOC(uint8_t SOC)
{
	#define voltageTableLength 32
	const static int16_t voltageTable [voltageTableLength] PROGMEM={	2000,2269,2383,2451,2473,2482,2489,2504,2517,2529,2539,
									2547,2555,2563,2571,2578,2585,2592,2600,2607,2613,2621,
									2630,2639,2651,2662,2677,2694,2716,2737,2755,2799 };
	#define tableStep ((uint8_t)((1<<8)/voltageTableLength))
	
	uint16_t cvl = pgm_read_word(&(voltageTable[SOC/tableStep]));
	//if the SOC maps to a voltage in the table, just lookup the voltage - easy
	if((SOC % tableStep) == 0)
	{
		return cvl;
	}
	
	//else, things aren't so easy - we need to do an interpolation
	//           y_2 - y_1
	//y = y_1 + ---------- (x - x_1)
	//           x_2 - x_1
	// let y_1 be the voltage below the SOC we want == cvl
	// let y_2 be the voltage above the SOC we want == cvu
	// let x_1 be floor(SOC/voltageTableLength)*voltageTableLength == SOCL
	// let x_2 be ceil(SOC/voltageTableLength)*voltageTableLength == SOCL + UINT8_MAX/voltageTableLength == SOCU
	// let x be the input SOC 
	//              cvu - cvl
	// CV = cv1 + ------------ (SOC - SOCL) = cv1 + (cvu - cvl)*(SOC - SOCL)*UINT8_MAX/voltageTableLength
	//             SOCU - SOCL
	uint16_t cvu = pgm_read_word(&(voltageTable[SOC/tableStep+1]));
	int16_t CV = cvl;
	int16_t SOCL = (SOC/tableStep)*tableStep;
	CV += ((int16_t)(cvu - cvl)*(SOC - SOCL))/tableStep;
	return CV;
	
}


bool allCellsAtBottom(CVRegPacket6802 *voltagePacket, uint8_t numberOfCells, uint16_t bottom)
{
	bool allCellsAtBottom = true;
	for(int i = 1; i <= numberOfCells; i++)
	{
		if(voltageFromCVReg(voltagePacket->reg.payload[0].bytes, i) >= bottom)
		{
			allCellsAtBottom = false;
		}
	}
	return allCellsAtBottom;
}



/*
uint16_t CellVoltageSum(CVRegPacket6802* voltagePacket, uint8_t numberOfCells)
{
	uint16_t sum = 0;
	for(int i = 1; i <= numberOfCells; i++)
	{
		sum += voltageFromCVReg(voltagePacket, i);
	}
	return sum;
}
*/

void print680xCV(int16_t LT680xCount)
{
	//each count is worth 1.5mV.  To convert to number of mV (in fixed point)
	// 1mV = 3/2 * count
	// + 0.5mV if cout % 2 == 1
	int16_t mv = (3*LT680xCount)/2;	
	
	int16_t unit = mv/1000;
	uint16_t decimal = mv%1000;
	
	//printf("%i", LT680xCount);
	printf_P(PSTR("%1i.%03u%c\t"), unit, decimal, ((LT680xCount % 2) ? '5' : '0'));
}

void printPowerSupplyStackVString(CVRegPacket6802* voltagePacket, uint8_t numberOfCells, char* currentString)
{
	uint16_t sum = 0;
	for(int i = 1; i <= numberOfCells; i++)
	{
		sum += voltageFromCVReg(voltagePacket->reg.payload[0].bytes, i);
	}
	sum += 466;	//there's a schottkey diode drop from the PS to the cells: 0.55V/1.5mV = 367
	uint16_t mv = 3*(sum/2) + (sum % 2);
	
	printf_P(PSTR("<ps<P%1i.%03i%cV%sRG>>\n"), mv/1000, mv%1000, ((sum % 2) ? '5' : '0'), currentString);
	
}



//selection sort algorithm
//from http://en.wikipedia.org/wiki/Selection_sort
void sortInt32(int32_t* data, uint8_t size)
{
	int iPos;
	int iMin;
	int32_t temp;
	
	for(iPos = 0; iPos < size; iPos++)
	{
		iMin = iPos;
		for(int i = iPos+1; i < size; i++)
		{
			if(data[i] < data[iMin])
				iMin = i;
		}
		
		if(iMin != iPos)
		{
			temp = data[iPos];
			data[iPos] = data[iMin];
			data[iMin]=temp;
		}
	}
}

bool doneCharging(CVRegPacket6802* voltageRegisters, uint8_t SOC)
{
	int16_t CVLim;
	if(SOC == UINT8_MAX)
	{
		CVLim = overVoltage;
	} else {
		CVLim = voltageFromSOC(SOC);
	}
	int16_t count;
	bool doneCharging = false;
	
	for(uint8_t i = 1; i <= NumberOfCells; i++)
	{
		count = voltageFromCVReg(voltageRegisters->reg.payload[0].bytes, i);
		//Test the index against the Masked cells
		//take 1 left shift by i-1 to get the bitfield for
		//that cell then and with the cell mask if the result
		//is 1, we don't want to check that cell
		if(!((1 << (i-1)) & MaskedCells))
		{
			if(count >= CVLim)	//done charging - quit charging
			{
				//puts_P(PSTR("<ps<S>>\n"));
				//puts_P(PSTR("Charging done\n"));
				doneCharging=true;
			}
		}		
	}
	return doneCharging;
}

bool doneDischarging(CVRegPacket6802* voltageRegisters, uint8_t SOC)
{
	int16_t CVLim = voltageFromSOC(SOC);
	
	int16_t count;
	for(uint8_t i = 1; i <= NumberOfCells; i++)
	{
		count = voltageFromCVReg(voltageRegisters->reg.payload[0].bytes, i);
		
		//Test the index against the Masked cells
		//take 1 left shift by i-1 to get the bitfield for
		//that cell then and with the cell mask if the result
		//is 1, we don't want to check that cell
		if(count <= CVLim && !isSet(i-1, MaskedCells))
		{
			return true;
		}	
	}
	return false;
}

bool isSet(uint8_t bit, uint16_t bitString)
{
	if((1 << bit) & bitString)
		return true;
	else
		return false;
}

void emergencyShutdown()
{
	//send command to stop power supply (will need to be relayed by PC via GPIB)
	//syntax is that of a HP6034A power supply (with << and >> that will be removed
	//by the PC before forwarding to the power supply)
	puts_P(PSTR("<ps<S>>\n"));
	puts_P(PSTR("<al<ISET 0>>\n"));
	puts_P(PSTR("<al<LOAD 0>>\n"));
	
	//disconnect stack from supplies
	DisableStackPort &= ~(1 << DisableStackPin);
	BalancerDisconnectPort &= ~(1 << BalancerDisconnect);
	
	CfgRegPacket6802 configRegisters;
	
	DischargerStatus = 0x00;
	updateCFGReg = true;
	
	configRegisters.packet.payload[0].reg.CFGR1=(DischargerStatus & 0x00FF);
	configRegisters.packet.payload[0].reg.CFGR2 &= 0xF0;
	configRegisters.packet.payload[0].reg.CFGR2 |= ((DischargerStatus & 0x0F00) >> 8);
	configRegisters.packet.command = WRCFG;
	
	while(SPIStatus.TransferInProgress)	//finish up whatever you were doing
		;
		
	startSPITransaction(configRegisters.bytes, sizeof(configRegisters.bytes), noFlags);
	
	ErrorLEDPort &= ~(1 << ErrorLED);
}

void shutdown()
{
	//send command to stop power supply (will need to be relayed by PC via GPIB)
	//syntax is that of a HP6034A power supply (with << and >> that will be removed
	//by the PC before forwarding to the power supply)
	puts_P(PSTR("<ps<S>>\n"));
	puts_P(PSTR("<al<ISET 0>>\n"));
	puts_P(PSTR("<al<LOAD 0>>\n"));
	puts_P(PSTR("**Exit**\n"));

	//disconnect stack from supplies
	DisableStackPort &= ~(1 << DisableStackPin);
	BalancerDisconnectPort &= ~(1 << BalancerDisconnect);

	balanceMode = normalDisch;
	DischargerStatus = 0x00;
	updateCFGReg = true;

	ErrorLEDPort |= (1 << ErrorLED);
	BalanceLEDPort |= (1 << BalanceLED);
	CalibrateLEDPort |= (1 << CalibrateLED);
}

enum
{
	ReadADC0 = 1,
	ReadADC1,
};


void readISenseADC(int16_t* stackCurrent)
{
	static uint8_t ADCState = ReadADC0;
	//static uint8_t newADCValue = false;
	static uint16_t ADC0 = 0;
	static uint16_t ADC1 = 0;
	
	if(ADCSRA & (1 << ADIF))
	{
		//newADCValue = true;
		int32_t ADCTemp;
		ADCSRA &= ~(1 << ADIF);
		//if last time we had state ReadADC0, this reading was for that
		if(ADCState == ReadADC0)
		{
			ADCTemp = ADC;
			ADCTemp=ADCTemp+(ADCTemp * IMeasCalPosNum)/IMeasCalPosDem;
			ADC0 = (uint16_t)ADCTemp;
			ADCState = ReadADC1;
			ADMUX = (ADMUX & (0xE0)) | 1;
		} else {
			ADCTemp = ADC;
			ADCTemp = ADCTemp+(ADCTemp * IMeasCalNegNum)/IMeasCalNegDem;
			ADC1 = (uint16_t)ADCTemp;
			ADCState = ReadADC0;
			ADMUX = (ADMUX & (0xE0)) | 0;
		}
		
		ADCSRA |= (1 << ADSC);	//start another conversion
	}
	#define negativeThreshold 0x02
	if(ADC0 >= negativeThreshold)
		*stackCurrent = ADC0;
	else if(ADC1 >= negativeThreshold)
		*stackCurrent = -ADC1;
	else
		*stackCurrent = 0;	//undefined case - both are 0
}
