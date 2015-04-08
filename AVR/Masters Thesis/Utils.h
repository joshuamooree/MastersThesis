#ifndef _utils_h_
#define _utils_h_
#include "GlobalDefs.h"

#include <LT6802.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "LT6802.h"
#include "Masters Thesis.h"
#include <avr/pgmspace.h>
#include <avr/io.h>

uint8_t getSOC(int16_t CV);
int16_t voltageFromSOC(uint8_t SOC);

bool allCellsAtBottom(CVRegPacket6802 *voltagePacket, uint8_t numberOfCells, uint16_t bottom);
//uint16_t CellVoltageSum(CVRegPacket6802 * voltagePacket, uint8_t numberOfCells);

void print680xCV(int16_t LT680xCV);
void printPowerSupplyStackVString(CVRegPacket6802* voltagePacket, uint8_t numberOfCells, char* currentString);

void sortInt32(int32_t* data, uint8_t size);

bool doneCharging(CVRegPacket6802* voltageRegisters, uint8_t SOC);
bool doneDischarging(CVRegPacket6802* voltageRegisters, uint8_t SOC);

bool isSet(uint8_t bit, uint16_t bitString);
void emergencyShutdown();
void shutdown();

void readISenseADC(int16_t* stackCurrent);


#define overVoltage  ((int16_t)(cellOV/LT6802CountValue))
#define underVoltage ((int16_t)(cellUV/LT6802CountValue))

#endif