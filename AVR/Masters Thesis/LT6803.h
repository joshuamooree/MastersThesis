#ifndef LT6803_h
#define LT6803_H

/*
Before including this file, you must define LengthOf6803Chain
It should look something like
#define LengthOf6803Chain 1
#include "LT6803.h"
*/
#ifndef LengthOf6803Chain
	#error "You must define LengthOf6803Chain before including this file"
#endif
//The indexing for arrays that involve data from multiple 6803's
//is  0 = top, 1 = top - 1, ..., LengthOf6803Chain = bottom

#include <inttypes.h>
#include <stdbool.h>


enum cmdCode6803
{
	WRCFG = 0x01,			//Write configuration register (CFGreg6803 structure)
	RDCFG = 0x02,			//Read configuration register (CFGreg6803 structure)
	RDCV = 0x04,			//Read all cell voltages
	RDCVA = 0x06,			//Read Cell Voltage 1-4
	RDCVB = 0x08,			//Read Cell Voltage 5-8
	RDCVC = 0x0A,			//Read cell voltages 9-12
	RDFLG = 0x0C,			//Read Flag registers (FLGReg6803 structure)
	RDTMP = 0x0E,			//Read Temp Registers (TMPReg6803 structure)
	STCVADAll = 0x10,		//Start Cell Voltage ADC Conversions and Poll status for all cells
	STCVAD1 = 0x11,			//Start Cell Voltage ADC Conversions and Poll status for cell 1
	STCVAD2 = 0x12,			//Start Cell Voltage ADC Conversions and Poll status for cell 2
	STCVAD3 = 0x13,			//Start Cell Voltage ADC Conversions and Poll status for cell 3
	STCVAD4 = 0x14,			//Start Cell Voltage ADC Conversions and Poll status for cell 4
	STCVAD5 = 0x15,			//Start Cell Voltage ADC Conversions and Poll status for cell 5
	STCVAD6 = 0x16,			//Start Cell Voltage ADC Conversions and Poll status for cell 6
	STCVAD7 = 0x17,			//Start Cell Voltage ADC Conversions and Poll status for cell 7
	STCVAD8 = 0x18,			//Start Cell Voltage ADC Conversions and Poll status for cell 8
	STCVAD9 = 0x19,			//Start Cell Voltage ADC Conversions and Poll status for cell 9
	STCVAD10 = 0x1A,		//Start Cell Voltage ADC Conversions and Poll status for cell 10
	STCVAD11 = 0x1B,		//Start Cell Voltage ADC Conversions and Poll status for cell 11
	STCVAD12 = 0x1C,		//Start Cell Voltage ADC Conversions and Poll status for cell 12
	STCVADClear = 0x1D,		//Clear ADC conversion results
	STCVADSelfTest1 = 0x1E,	//Start Self test 1
	STCVADSelfTest2 = 0x1F,	//start Self test 2
	
	STOWADAll = 0x20,		//Start Open Wire ADC Conversions and Poll Status for all cells
	STOWAD1 = 0x21,			//Start Open Wire ADC Conversions and Poll Status for cell 1
	STOWAD2 = 0x22,			//Start Open Wire ADC Conversions and Poll Status for cell 2
	STOWAD3 = 0x23,			//Start Open Wire ADC Conversions and Poll Status for cell 3
	STOWAD4 = 0x24,			//Start Open Wire ADC Conversions and Poll Status for cell 4
	STOWAD5 = 0x25,			//Start Open Wire ADC Conversions and Poll Status for cell 5
	STOWAD6 = 0x26,			//Start Open Wire ADC Conversions and Poll Status for cell 6
	STOWAD7 = 0x27,			//Start Open Wire ADC Conversions and Poll Status for cell 7
	STOWAD8 = 0x28,			//Start Open Wire ADC Conversions and Poll Status for cell 8
	STOWAD9 = 0x29,			//Start Open Wire ADC Conversions and Poll Status for cell 9
	STOWAD10 = 0x2A,		//Start Open Wire ADC Conversions and Poll Status for cell 10
	STOWAD11 = 0x2B,		//Start Open Wire ADC Conversions and Poll Status for cell 11
	STOWAD12 = 0x2C,		//Start Open Wire ADC Conversions and Poll Status for cell 12
	
	STTMPADAll = 0x30,		//Start Temperature ADC Conversions and Poll Status for all cells
	STTMPADExt1 = 0x31,		//Start Temperature ADC Conversions and Poll Status for external 1
	STTMPADExt2 = 0x32,		//Start Temperature ADC Conversions and Poll Status for external 2
	STTMPADInt = 0x33,		//Start Temperature ADC Conversions and Poll Status for internal
	STTMPADSelfTest1 = 0x3E,//Start Temperature ADC Conversions and Poll Status for self test 1
	STTMPADSelfTest2 = 0x3F,//Start Temperature ADC Conversions and Poll Status for self test 2
	
	PLADC = 0x40,			//Poll ADC Converter status
	
	PLINT = 0x50,			//Poll Interrupt status
	
	DAGN = 0x52,			//Start Diagnose and Poll Status
	
	RDDGNR = 0x54,			//Read Diagnostic register (use DGNRReg6803 structure)
	
	STCVDCAll = 0x60,		//Start Cell Voltage ADC Conversions and poll status, discharge permitted, all cells
	STCVDC1 = 0x61,			//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 1
	STCVDC2 = 0x62,			//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 2
	STCVDC3 = 0x63,			//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 3
	STCVDC4 = 0x64,			//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 4
	STCVDC5 = 0x65,			//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 5
	STCVDC6 = 0x66,			//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 6
	STCVDC7 = 0x67,			//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 7
	STCVDC8 = 0x68,			//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 8
	STCVDC9 = 0x69,			//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 9
	STCVDC10 = 0x6A,		//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 10
	STCVDC11 = 0x6B,		//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 11
	STCVDC12 = 0x6C,		//Start Cell Voltage ADC Conversions and poll status, discharge permitted, cell 12
	
	STOWDCAll = 0x70,		//Start Open Wire ADC Conversions and poll status, discharge permitted, all cells
	STOWDC1 = 0x71,			//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 1
	STOWDC2 = 0x72,			//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 2
	STOWDC3 = 0x73,			//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 3
	STOWDC4 = 0x74,			//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 4
	STOWDC5 = 0x75,			//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 5
	STOWDC6 = 0x76,			//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 6
	STOWDC7 = 0x77,			//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 7
	STOWDC8 = 0x78,			//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 8
	STOWDC9 = 0x79,			//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 9
	STOWDC10 = 0x7A,		//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 10
	STOWDC11 = 0x7B,		//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 11
	STOWDC12 = 0x7C,		//Start Open Wire ADC Conversions and poll status, discharge permitted, cell 12
};

/*
typedef struct
{
	uint8_t CFGR0;  //watchdog timer mode, gpio 1, 2,, level poll mode, 10 cell mode, comparator duty cycle
	uint8_t CFGR1;	//Discharge cell bits 8-1
	uint8_t CFGR2;  //discharge cell bits 12-9, mask cell interrupts, bits 1-4
	uint8_t CFGR3;  //mask cell interrupts, bits 12-5
	uint8_t CFGR4;  //undervoltage comparison voltage byte
	uint8_t CFGR5;  //overvoltage comparison byte
} CFGReg6803Struct;
*/

typedef struct
{
	//CFGR0
	bool WDT    : 1;
	bool GPIO2  : 1;
	bool GPIO1  : 1;
	bool LVLPL  : 1;
	bool CELL10 : 1;
	unsigned int CDC : 3;
	
	//CFGR1
	bool DCC8 : 1;
	bool DCC7 : 1;
	bool DCC6 : 1;
	bool DCC5 : 1;
	bool DCC4 : 1;
	bool DCC3 : 1;
	bool DCC2 : 1;
	bool DCC1 : 1;
	
	//CFGR2
	bool MC4I : 1;
	bool MC3I : 1;
	bool MC2I : 1;
	bool MC1I : 1;
	bool DCC12 : 1;
	bool DCC11 : 1;
	bool DCC10 : 1;
	bool DCC9 : 1;
	
	//CFGR3
	bool MC12I : 1;
	bool MC11I : 1;
	bool MC10I : 1;
	bool MC9I  : 1;
	bool MC8I  : 1;
	bool MC7I  : 1;
	bool MC6I  : 1;
	bool MC5I  : 1;
	
	//CFGR4
	uint8_t VUV;
	
	//CFGR5
	uint8_t VOV;
	
	//PEC
	uint8_t PEC;
	
} CFGReg6803Struct;


typedef union
{
	CFGReg6803Struct reg;
	uint8_t bytes [sizeof(CFGReg6803Struct)];
} CFGReg6803;

typedef struct 
{
	uint8_t command;
	uint8_t PEC;
	CFGReg6803 payload [LengthOf6803Chain];
} CfgRegPacket6803Struct;

typedef struct
{
	CfgRegPacket6803Struct packet;
	uint8_t bytes[sizeof(CfgRegPacket6803Struct)];
} CfgRegPacket6803;

typedef struct
{
	uint8_t CVR0;	//cell 1 voltage, bits 7-0
	uint8_t CVR1;   //cell 2 voltage, bits 3-0, cell 1 bits 11-8
	uint8_t CVR2;   //cell 2 voltage, bits 11-4
	uint8_t CVR3;   //cell 3 voltage, bits 7-0
	uint8_t CVR4;   //cell 4 voltage, bits 3-0, cell 3 bits 11-8
	uint8_t CVR5;
	uint8_t CVR6;
	uint8_t CVR7;
	uint8_t CVR8;
	uint8_t CVR9;
	uint8_t CVR10;
	uint8_t CVR11;
	uint8_t CVR12;
	uint8_t CVR13;
	uint8_t CVR14;
	uint8_t CVR15;
	uint8_t CVR16;
	uint8_t CVR17;
	uint8_t PEC;
} CVRReg6803Struct;

typedef union
{
	CVRReg6803Struct reg;
	uint8_t bytes [sizeof(CVRReg6803Struct)];
} CVRReg6803;

typedef struct
{
	uint8_t command;
	uint8_t PEC;
	CVRReg6803 payload [LengthOf6803Chain];
} CVRegPacket6803;

//************************************
// Method:    voltageFromCVReg
// FullName:  voltageFromCVReg
// Access:    public 
// Returns:   int16_t, where each count is worth 1.5mV.  Negative values are reported as a negative number.
// Qualifier:
// Parameter: CVRReg6803 * reg
// Parameter: uint8_t cell
//************************************
int16_t voltageFromCVReg(CVRReg6803* reg, uint8_t cell )
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
		msbMask = 0x1F;
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
		lsbShiftValue = -4;
		lsbMask = 0xE0;
		msbMask = 0xFF;
		//this index is the same as the other case, except we subtract one from
		//the cell number then add one at the end (since cell 2 starts at index 1, etc)
		regIndex = ((cell-2)/2)*3+1;
	}		
	
	ADCCount = ((reg->bytes[regIndex]&&lsbMask) << lsbShiftValue) + ((reg->bytes[regIndex+1]&&msbMask) << msbShiftValue);
	
	return (int16_t)ADCCount - 512;

}

typedef struct 
{
	uint8_t FLGR0;
	uint8_t FLGR1;
	uint8_t FLGR2;
} FLGReg6803Struct;

typedef union
{
	FLGReg6803Struct reg;
	uint8_t bytes [sizeof(FLGReg6803Struct)];
} FLGReg6803;

typedef struct
{
	uint8_t TMPR0;
	uint8_t TMPR1;
	uint8_t TMPR2;
	uint8_t TMPR3;
	uint8_t TMPR4;
} TMPRReg6803Struct;

typedef union
{
	TMPRReg6803Struct reg;
	uint8_t bytes[sizeof(TMPRReg6803Struct)];
} TMPRReg6803;

typedef struct  
{
	uint8_t PEC;
} PECReg6803Struct;

typedef union
{
	PECReg6803Struct reg;
	uint8_t bytes [sizeof(PECReg6803Struct)];
} PECReg6803;

typedef struct  
{
	uint8_t DGNR0;
	uint8_t DGNR1;
} DGNRReg6803Struct;

typedef union
{
	DGNRReg6803Struct reg;
	uint8_t bytes[sizeof(DGNRReg6803Struct)];
} DGNRReg6803;

uint8_t CRC86803 (uint8_t* data, uint8_t length)
{
	//as it turns out, a big endian crc 8 with characteristic polynomial x^8 + x^2 + x + 1
	//can be caculated as 
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
	uint8_t C=0x41;	//initial value of 0x41 as specified by LT6803
	uint8_t E;
	for (int i = 0; i < length; i++)
	{
		E=data[i]^C;
		C=E ^ (E << 1) ^ (E << 2) ^ ((E >> 4) & 0x0C) ^ ((E >> 5) & 0x02) ^ ((E >> 6) & 0x01) ^ (E >> 7);
	}
	
	return C;
}

#endif