#define Dash2

#include "LT6802.h"

//************************************
// Method:    voltageFromCVReg
// FullName:  voltageFromCVReg
// Access:    public 
// Returns:   int16_t, where each count is worth 1.5mV.  Negative values are reported as a negative number.
// Qualifier:
// Parameter: CVRReg6802 * reg
// Parameter: uint8_t cell
//************************************
int16_t voltageFromCVReg(uint8_t* reg, uint8_t cell )
{
	//the way the register is setup, each cell has 12 bits, so two cells are packed into 24 bits (3 bytes).
	//so, cell 1 is packed with its 8 lsbs in register 1, and its 4 msbs as the lsb in register 2.  cell 2
	//has its 4 lsbs in the msbs of register 2 and its 8 msbs in register 3.
	
	uint8_t msbShiftValue;
	int8_t lsbShiftValue;
	uint8_t msbMask;
	uint8_t lsbMask;
	uint8_t regIndex;
	
	uint16_t ADCCount;
	
	if( cell == 0 || cell > 12)	//invalid cell number
		return 0;
		
	if(cell % 2 == 1)
	{
		msbShiftValue = 8;
		lsbShiftValue = 0;
		lsbMask = 0xFF;
		msbMask = 0x0F;
		//the way this indexing is derived:
		//cell	index	index/3	(cell-1)/2
		//1		0		0		0
		//3		3		1		1
		//5		6		2		2
		//7		9		3		3
		//9		12		4		4
		//11	15		5		5
		regIndex = ((cell-1)/2)*3;
	}		
	else
	{
		msbShiftValue = 4;
		lsbShiftValue = 4;
		lsbMask = 0xF0;
		msbMask = 0xFF;
		//this index is the same as the other case, except we subtract one from
		//the cell number then add one at the end (since cell 2 starts at index 1, etc)
		regIndex = ((cell-2)/2)*3+1;
	}		
	
	ADCCount = ((reg[regIndex] & lsbMask) >> lsbShiftValue) + ((reg[regIndex+1] & msbMask) << msbShiftValue);
	
	#ifdef LT6802
	return (int16_t)ADCCount;
	#else
	return (int16_t)ADCCount - 512;
	#endif
}

void voltageToCVReg(uint8_t* reg, uint8_t cell, int16_t voltage )
{
	//the way the register is setup, each cell has 12 bits, so two cells are packed into 24 bits (3 bytes).
	//so, cell 1 is packed with its 8 lsbs in register 1, and its 4 msbs as the lsb in register 2.  cell 2
	//has its 4 lsbs in the msbs of register 2 and its 8 msbs in register 3.
	
	//uint8_t msbShiftValue;
	//int8_t lsbShiftValue;
	//uint8_t msbMask;
	//uint8_t lsbMask;
	uint8_t regIndex;
	
	#ifndef LT6802
	voltage += 512;	//LT6803 uses count 512 as 0V.  Its easier for us to use signed integers, so adjust things.
	#endif
	
	//clear any extraneous bits
	voltage &= 0x0FFF;
	
	if( cell == 0 || cell > 12)	//invalid cell number
		return;
	
	if(cell % 2 == 1)
	{
		//msbShiftValue = 8;
		//lsbShiftValue = 0;
		//lsbMask = 0xFF;
		//msbMask = 0x0F;
		//regIndex = ((cell-1)/2)*3;
		
		//the way this indexing is derived:
		//cell	index	index/3	(cell-1)/2
		//1		0		0		0
		//3		3		1		1
		//5		6		2		2
		//7		9		3		3
		//9		12		4		4
		//11	15		5		5
		regIndex = ((cell-1)>>1)*3;
		
		reg[regIndex] = (uint8_t)~0xFF;
		reg[regIndex+1] &= (uint8_t)~0x0F;
		reg[regIndex] = (uint8_t)(voltage << 0);
		reg[regIndex+1] |= (voltage >> 8 ) & 0x0F;
	}		
	else
	{
		//msbShiftValue = 4;
		//lsbShiftValue = 4;
		//lsbMask = 0xF0;
		//msbMask = 0xFF;
		//this index is the same as the other case, except we subtract one from
		//the cell number then add one at the end (since cell 2 starts at index 1, etc)
		//regIndex = ((cell-2)/2)*3+1;
		
		regIndex = ((cell-2)>>1)*3+1;
		
		reg[regIndex] &= (uint8_t)~0xF0;
		reg[regIndex+1] = (uint8_t)~0xFF;
		reg[regIndex] |= (voltage << 4) & 0xF0;
		reg[regIndex+1] = (uint8_t)(voltage >> 4 );
	}
	
	//voltage &= 0x0FFF;
	//reg[regIndex] &= ~lsbMask;
	//reg[regIndex+1] &= ~msbMask;
	//reg[regIndex] |= (voltage << lsbShiftValue) & lsbMask;
	//reg[regIndex+1] |= (voltage >> msbShiftValue ) & msbMask;
}

uint8_t CRC86802 (uint8_t* data, uint8_t length)
{
	//as it turns out, a big endian crc 8 with characteristic polynomial x^8 + x^2 + x + 1
	//can be calculated as 
	//let Cx = register start bit x
	//let Dx = data input start bit x
	//let Ex = Cx + Dx
	//then
	//result bit		forumula
	//   7				E7 + E6 + E5
	//   6				E6 + E5 + E4
	//	 5				E5 + E4 + E3
	//	 4				E4 + E3 + E2
	//	 3				E3 + E2 + E1 + E7
	//	 2				E2 + E1 + E0 + E6
	//	 1				E1 + E0      + E6
	//	 0				E0			 + E6 + E7
	#ifdef LT6802
	uint8_t C=0x00;	//proper initial value for LT6802 (not explicitly specified by datasheet)
	#else
	uint8_t C=0x41;	//initial value of 0x41 as specified by LT6803
	#endif
	
	uint8_t E;
	for (int i = 0; i < length; i++)
	{
		E=data[i]^C;
		C=E ^ (E << 1) ^ (E << 2) ^ ((E >> 4) & 0x0C) ^ ((E >> 5) & 0x02) ^ ((E >> 6) & 0x01) ^ (E >> 7);
	}
	
	return C;
}