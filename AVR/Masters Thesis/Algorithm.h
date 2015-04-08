#ifndef _ALGORITHM_H_
#define _ALGORITHM_H_
#include "GlobalDefs.h"
#include "Masters Thesis.h"
#include "LT6802.h"
#include <stdio.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#ifndef LC_INCLUDE
#define LC_INCLUDE "lc-addrlabels.h"
#endif
#include "pt/pt.h"

#include "Utils.h"

enum Modes {
	DoNothing,
	JustLog,
	ExtractParams,
	DoBalance,
	DoTopBalance,
	ChargeCells,
	DischargeCells,
	FindCapacities,
	ActiveBalanceChargeDischarge,
	Test,
	Done
};

typedef struct
{
	uint8_t mode;	//mode selects a "program".  A program is a series of actions to take.
	uint8_t action;	//state holds which action is currently being taken.
	CVRegPacket6802* voltagePacket;
} AlgorithmState;


#define RScale 100
#define CScale 100

#define BalanceEff 70

#define FullCharge UINT8_MAX
#define FullDischarge 0

#define InitBalanceTolerance 500	//  =1/300 = 0.3%
void AlgorithmMainLoop (AlgorithmState* state);

#define PulseSettleThreshold 1	//in LT6802 ADC ticks
#define PulseSettleTime 10	//seconds
#define PulseWidth 300
#define multiCellBalance	//comment out to get one cell at a time

typedef struct 
{
	struct pt ptVar;
	AlgorithmState* state;
} threadVar;

int16_t getOCV(CVRegPacket6802* voltages, uint8_t cell);

void calcIBDCHMult();
bool cellsBalanced(CVRegPacket6802* voltagePacket, uint8_t Tolerance);

PT_THREAD(calibrateTopBalance(threadVar* var, uint8_t SOC));
PT_THREAD(calibrateBottomBalance(threadVar* var));
PT_THREAD(pulseTest(threadVar* var));
PT_THREAD(charge(threadVar* var, const char* current, uint8_t SOC));
PT_THREAD(discharge(threadVar* var, const char* rate, uint8_t SOC));
PT_THREAD(masterThread(threadVar* var));
PT_THREAD(delay(threadVar* var, uint16_t delay_s));
PT_THREAD(updateOCV(struct pt* pt));
PT_THREAD(activeBalanceMonitor(struct pt* pt));

enum ActiveBalanceModes
{
	ActiveBalanceOff,
	ActiveBalanceOn,
	ActiveBalanceEnd,
	ActiveBalanceBegin
};


#endif
