#include "TerrainComp.h"

double correctedLatitude = 0.0;
double correctedLongitude = 0.0;
float correctedAltitude = 0.0;
bool terrainCompValid = false;

float imuRollDeg = 0.0;
float imuPitchDeg = 0.0;

static float filteredRollDeg = 0.0;
static float filteredPitchDeg = 0.0;
static bool filtersInitialized = false;
static unsigned long lastTerrainCompMs = 0;

SemaphoreHandle_t i2cMutex = NULL;

bool takeI2cMutex(TickType_t waitMs)
{
  if (i2cMutex == NULL)
    return true;

  return (xSemaphoreTake(i2cMutex, waitMs / portTICK_PERIOD_MS) == pdPASS);
}

void giveI2cMutex()
{
  if (i2cMutex != NULL)
    xSemaphoreGive(i2cMutex);
}

void beginI2cMutex()
{
  if (i2cMutex == NULL)
    i2cMutex = xSemaphoreCreateMutex();
}

static float lowPassFilter(float previous, float sample, float alpha)
{
  if (!filtersInitialized)
    return sample;

  return previous + alpha * (sample - previous);
}

static float filterAlphaFromHz(float filterHz, float sampleRateHz)
{
  if (filterHz <= 0.0f || sampleRateHz <= 0.0f)
    return 1.0f;

  float dt = 1.0f / sampleRateHz;
  float rc = 1.0f / (2.0f * PI * filterHz);
  return dt / (rc + dt);
}

void resetTerrainCompFilters()
{
  filtersInitialized = false;
  filteredRollDeg = 0.0f;
  filteredPitchDeg = 0.0f;
}

void applyTerrainComp()
{
  if (!settings.enableTerrainComp || !online.bno086)
  {
    correctedLatitude = latitude;
    correctedLongitude = longitude;
    correctedAltitude = altitude;
    terrainCompValid = (online.gnss && fixType >= 2);
    return;
  }

  if (fixType < 2)
  {
    terrainCompValid = false;
    return;
  }

  float rollDeg = imuRollDeg - settings.terrainRollOffsetDeg;
  float pitchDeg = imuPitchDeg - settings.terrainPitchOffsetDeg;

  float alpha = filterAlphaFromHz(settings.terrainAttitudeFilterHz, 50.0f);
  filteredRollDeg = lowPassFilter(filteredRollDeg, rollDeg, alpha);
  filteredPitchDeg = lowPassFilter(filteredPitchDeg, pitchDeg, alpha);
  filtersInitialized = true;

  float rollRad = filteredRollDeg * DEG_TO_RAD;
  float pitchRad = filteredPitchDeg * DEG_TO_RAD;
  float h = settings.poleHeightM;

  // Horizontal displacement of antenna from ground point below a vertical pole.
  double deltaE = h * sin(pitchRad);
  double deltaN = h * sin(rollRad);
  double deltaU = h * (cos(pitchRad) * cos(rollRad) - 1.0);

  const double metersPerDegLat = 111320.0;
  double metersPerDegLon = metersPerDegLat * cos(latitude * DEG_TO_RAD);

  double newLat = latitude - (deltaN / metersPerDegLat);
  double newLon = longitude - (deltaE / metersPerDegLon);
  float newAlt = altitude + (float)deltaU;

  unsigned long nowMs = millis();
  if (settings.terrainPositionSmoothTauS > 0.0f && terrainCompValid && lastTerrainCompMs > 0)
  {
    float dt = (nowMs - lastTerrainCompMs) / 1000.0f;
    float tau = settings.terrainPositionSmoothTauS;
    float posAlpha = dt / (tau + dt);

    correctedLatitude = correctedLatitude + posAlpha * (newLat - correctedLatitude);
    correctedLongitude = correctedLongitude + posAlpha * (newLon - correctedLongitude);
    correctedAltitude = correctedAltitude + posAlpha * (newAlt - correctedAltitude);
  }
  else
  {
    correctedLatitude = newLat;
    correctedLongitude = newLon;
    correctedAltitude = newAlt;
  }

  lastTerrainCompMs = nowMs;
  terrainCompValid = true;
}

void terrainCompLevelZero()
{
  settings.terrainRollOffsetDeg = imuRollDeg;
  settings.terrainPitchOffsetDeg = imuPitchDeg;
  resetTerrainCompFilters();
  recordSystemSettings();
  Serial.println(F("Terrain level zero captured"));
}
