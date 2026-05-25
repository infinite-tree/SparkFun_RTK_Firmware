#pragma once

// Terrain compensation: project antenna LLH to ground point using BNO086 attitude.

extern double correctedLatitude;
extern double correctedLongitude;
extern float correctedAltitude;
extern bool terrainCompValid;

extern float imuRollDeg;
extern float imuPitchDeg;

void resetTerrainCompFilters();
void applyTerrainComp();
void terrainCompLevelZero();

bool patchNmeaSentence(char *sentence);
void patchNmeaBuffer(uint8_t *buffer, size_t length);
bool buildCorrectedGga(char *outBuffer, size_t outSize);

void beginI2cMutex();
#ifdef COMPILE_TERRAIN_COMP
void beginBNO086();
#endif

bool takeI2cMutex(TickType_t waitMs);
void giveI2cMutex();
