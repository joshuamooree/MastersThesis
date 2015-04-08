#ifndef _MASTERS_THESIS_
#define _MASTERS_THESIS_
#include "GlobalDefs.h"
#include <inttypes.h>
#include <stdbool.h>
#include "LT6802.h"
#include "Utils.h"


#ifndef LC_INCLUDE
#define LC_INCLUDE "lc-addrlabels.h"
#endif
#include "pt/pt.h"



extern volatile uint16_t DischargerStatus;	//bit field representing state of each discharger.  1 = on,, 0 = off
extern volatile uint8_t oneSecondPassed;
extern volatile uint8_t hundredMSPassed;
extern volatile uint32_t time;
extern volatile uint8_t NewADCReadings;
extern volatile uint8_t NewISenseReadings;
extern volatile bool updateCFGReg;
extern volatile int32_t currentAccumulators[NumberOfCells];
extern volatile uint8_t globalError;
extern volatile int16_t stackCurrent;
extern volatile uint8_t balanceMode;
extern volatile uint8_t balancePWMThresholds [NumberOfCells];
extern volatile bool force6802Conversion;
extern volatile bool inhibitStackIADC;

enum logModes
{
//	logSOC = 1,
	logRaw = 1,
	logOCV = 2,
	logBalancer = 4,
	logStackI = 8,
	logR1I = 16,
	logPWMThresh = 32,
	logNone = 64
};

enum balanceModes
{
	normalDisch,
	PWMDisch
};
extern uint8_t logMode;

extern uint16_t R [NumberOfCells];
extern uint16_t R1 [NumberOfCells];
extern uint16_t C1 [NumberOfCells];
extern double R1I [NumberOfCells];
extern int32_t capacities[NumberOfCells];

#define noFlags 0x00
#define SDOStatusBeforeCS 0x01
#define readFromSPI 0x02
void startSPITransaction (uint8_t* data, uint8_t size, uint8_t flags);


#define secondPrintMask 0x01
#define secondReadMask 0x02
#define secondAlgorithmMask 0x04
#define secondOCVMask 0x08

#define msAlgorithmMask 0x10


#define FOSCCal 1.042
#define Timer1OneSecondOffset (uint16_t)(F_CPU/1024*FOSCCal)
#define HundredMSOffset (uint16_t)(Timer1OneSecondOffset/10)

//definitions for NewADCReadings
#define ADCPrint 0x01
#define ADCAlgorithm 0x02
#define ADCBalanceFlag 0x04
#define ADCGlobalAlarm 0x08
#define ADCOCVVoltRead 0x10

//definitions for NewISenseReadings
#define IsenseFlag 0x01
#define ISensePrint 0x02
#define ISenseCalBalance 0x04
#define ISenseAlgorithm 0x08
#define ISenseUpdateOCV 0x10
#define ISenseAlgoMonitor 0x20




typedef struct  
{
	//semaphore - if true, do not touch anything in this structure.
	uint8_t TransferInProgress;	  //Do NOT write this field - the ISR or the setup function will do that.
	//data to be transferred
	uint8_t* Data;
	uint8_t DataLength;
	uint8_t currentByte;
	//these last two fields are to allow for level polling
	uint8_t SDOStatus;
	bool readSDOStatusBeforeCS;
	bool readDataFromSPI;
} SPIStatusStruct;


extern volatile SPIStatusStruct SPIStatus;

typedef struct  
{
	struct pt ptVar;
	CVRegPacket6802* voltageRegisters;
	//SPIStatusStruct* SPIStatus;
}commThreadVars;

PT_THREAD(LT6802CommThread(commThreadVars* vars));
PT_THREAD(ReadStackCurrent(struct pt* ptVars));
PT_THREAD(SoftVoltageCheck(commThreadVars* vars));

typedef struct
{
	struct pt ptVar;
	CVRegPacket6802* voltageRegisters;
}logThreadVars;

PT_THREAD(logData(logThreadVars* var));

#ifdef F_CPU
#undef F_CPU
#endif
#define F_CPU 8000000

#endif
