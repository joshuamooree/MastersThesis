#ifndef _LT6802_h_
#define _LT6802_h_


/*
Before including this file, you must define LengthOf6802Chain
It should look something like
#define LengthOf6802Chain 1
#include "LT6802.h"
*/
#ifndef LengthOf6802Chain
	#ifdef Dash2
		#define LengthOf6802Chain 1
	#else
		#error "You must define LengthOf6802Chain or Dash2 (indicating this is a LT6802-2) before including this file"
	#endif
#else
	#ifdef Dash2
		#error "You must define LengthOf6802Chain or Dash2, but not both."
	#endif
#endif
#define LT6802
//The indexing for arrays that involve data from multiple 6802's
//is  0 = top, 1 = top - 1, ..., LengthOf6802Chain = bottom

#include <inttypes.h>
#include <stdbool.h>


enum cmdCode6802
{
	WRCFG = 0x01,			//Write configuration register (CFGreg6802 structure)
	RDCFG = 0x02,			//Read configuration register (CFGreg6802 structure)
	RDCV = 0x04,			//Read all cell voltages
	RDCVA = 0x06,			//Read Cell Voltage 1-4
	RDCVB = 0x08,			//Read Cell Voltage 5-8
	RDCVC = 0x0A,			//Read cell voltages 9-12
	RDFLG = 0x0C,			//Read Flag registers (FLGReg6802 structure)
	RDTMP = 0x0E,			//Read Temp Registers (TMPReg6802 structure)
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
	
	RDDGNR = 0x54,			//Read Diagnostic register (use DGNRReg6802 structure)
	
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

#define WDT 7		//in CFGR0
#define GPIO2 6		//in CFGR0
#define GPIO1 5		//in CFGR0
#define LVLPL 4		//in CFGR0
#define CELL10 3	//in CFGR0
#define CDC2 2		//in CFGR0
#define CDC1 1		//in CFGR0
#define CDC0 0		//in CFGR0

#define DCC8 7		//in CFGR1
#define DCC7 6		//in CFGR1
#define DCC6 5		//in CFGR1
#define DCC5 4		//in CFGR1
#define DCC4 3		//in CFGR1
#define DCC3 2		//in CFGR1
#define DCC2 1		//in CFGR1
#define DCC1 0		//in CFGR1

#define MC4I 7		//in CFGR2
#define MC3I 6		//in CFGR2
#define MC2I 5		//in CFGR2
#define MC1I 4		//in CFGR2
#define DCC12 3		//in CFGR2
#define DCC11 2		//in CFGR2
#define DCC10 1		//in CFGR2
#define DCC9 0		//in CFGR2

#define MC12I 7		//in CFGR3
#define MC11I 6		//in CFGR3
#define MC10I 5		//in CFGR3
#define MC9I 4		//in CFGR3
#define MC8I 3		//in CFGR3
#define MC7I 2		//in CFGR3
#define MC6I 1		//in CFGR3
#define MC5I 0		//in CFGR3


typedef struct
{
	uint8_t CFGR0;  //watchdog timer mode, gpio 1, 2,, level poll mode, 10 cell mode, comparator duty cycle
	uint8_t CFGR1;	//Discharge cell bits 8-1
	uint8_t CFGR2;  //discharge cell bits 12-9, mask cell interrupts, bits 1-4
	uint8_t CFGR3;  //mask cell interrupts, bits 12-5
	uint8_t VUV;  //undervoltage comparison voltage byte
	uint8_t VOV;  //overvoltage comparison byte
} CFGReg6802Struct;



/*
typedef struct
{
	//CFGR0
	bool WDT    : 1;		//0 0
	bool GPIO2  : 1;		//0 1
	bool GPIO1  : 1;		//0 2
	bool LVLPL  : 1;		//0 3
	bool CELL10 : 1;		//0 4
	unsigned int CDC : 3;	//0 7
	
	//CFGR1
	bool DCC8 : 1;			//1 0
	bool DCC7 : 1;			//1 1
	bool DCC6 : 1;			//1 2
	bool DCC5 : 1;			//1 3
	bool DCC4 : 1;			//1 4
	bool DCC3 : 1;			//1 5
	bool DCC2 : 1;			//1 6
	bool DCC1 : 1;			//1 7
	
	//CFGR2
	bool MC4I : 1;			//2 0
	bool MC3I : 1;			//2 1
	bool MC2I : 1;			//2 2
	bool MC1I : 1;			//2 3
	bool DCC12 : 1;			//2 4
	bool DCC11 : 1;			//2 5
	bool DCC10 : 1;			//2 6
	bool DCC9 : 1;			//2 7
	
	//CFGR3
	bool MC12I : 1;			//3 0
	bool MC11I : 1;			//3 1
	bool MC10I : 1;			//3 2
	bool MC9I  : 1;			//3 3
	bool MC8I  : 1;			//3 4
	bool MC7I  : 1;			//3 5
	bool MC6I  : 1;			//3 6
	bool MC5I  : 1;			//3 7
	
	//CFGR4
	uint8_t VUV;			//4 
	
	//CFGR5
	uint8_t VOV;			//5
	
} CFGReg6802Struct;
*/

typedef union
{
	CFGReg6802Struct reg;
	uint8_t bytes [sizeof(CFGReg6802Struct)];
} CFGReg6802;

typedef struct 
{
	uint8_t command;
	CFGReg6802 payload [LengthOf6802Chain];
} CfgRegPacket6802Struct;

typedef union
{
	CfgRegPacket6802Struct packet;
	uint8_t bytes[sizeof(CfgRegPacket6802Struct)];
} CfgRegPacket6802;

#define LT6802CountValue 0.0015
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
} CVRReg6802Struct;

typedef union
{
	CVRReg6802Struct reg;
	uint8_t bytes [sizeof(CVRReg6802Struct)];
} CVRReg6802;

typedef struct
{
	#ifdef Dash2
		uint8_t address;
	#endif
	uint8_t command;
	CVRReg6802 payload [LengthOf6802Chain];
} CVRegPacket6802Struct;

typedef union 
{
	CVRegPacket6802Struct reg;
	uint8_t bytes[sizeof(CVRegPacket6802Struct)];
} CVRegPacket6802;

//************************************
// Method:    voltageFromCVReg
// FullName:  voltageFromCVReg
// Access:    public 
// Returns:   int16_t, where each count is worth 1.5mV.  Negative values are reported as a negative number.
// Qualifier:
// Parameter: CVRReg6802 * reg
// Parameter: uint8_t cell
//************************************
int16_t voltageFromCVReg(uint8_t* reg, uint8_t cell );

void voltageToCVReg(uint8_t* reg, uint8_t cell, int16_t voltage );


typedef struct 
{
	uint8_t FLGR0;
	uint8_t FLGR1;
	uint8_t FLGR2;
} FLGReg6802Struct;

typedef union
{
	FLGReg6802Struct reg;
	uint8_t bytes [sizeof(FLGReg6802Struct)];
} FLGReg6802;

typedef struct  
{
	#ifdef Dash2
		uint8_t address;
	#endif
	uint8_t command;
	FLGReg6802 packet;
} FLGRegPacket6802Struct;

typedef union
{
	FLGRegPacket6802Struct reg;
	uint8_t bytes[sizeof(FLGRegPacket6802Struct)];
} FLGRegPacket6802;

typedef struct
{
	uint8_t TMPR0;
	uint8_t TMPR1;
	uint8_t TMPR2;
	uint8_t TMPR3;
	uint8_t TMPR4;
} TMPRReg6802Struct;

typedef union
{
	TMPRReg6802Struct reg;
	uint8_t bytes[sizeof(TMPRReg6802Struct)];
} TMPRReg6802;

typedef struct  
{
	uint8_t PEC;
} PECReg6802Struct;

typedef union
{
	PECReg6802Struct reg;
	uint8_t bytes [sizeof(PECReg6802Struct)];
} PECReg6802;

typedef struct  
{
	uint8_t DGNR0;
	uint8_t DGNR1;
} DGNRReg6802Struct;

typedef union
{
	DGNRReg6802Struct reg;
	uint8_t bytes[sizeof(DGNRReg6802Struct)];
} DGNRReg6802;

uint8_t CRC86802 (uint8_t* data, uint8_t length);

typedef struct  
{
	uint8_t command;
} OneByteCommandPacketStruct;

typedef union
{
	OneByteCommandPacketStruct reg;
	uint8_t bytes[sizeof(OneByteCommandPacketStruct)];
} OneByteCommandPacket;

#ifdef Dash2
typedef struct 
{
	uint8_t address;
	uint8_t command;
} AddressCommandPacketStruct;

typedef union
{
	AddressCommandPacketStruct reg;
	uint8_t bytes[sizeof(AddressCommandPacketStruct)];
} AddressCommandPacket;
#endif

#endif