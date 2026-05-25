#include "TerrainComp.h"

static bool sentenceIsGga(const char *sentence)
{
  return (strncmp(sentence, "$GNGGA,", 7) == 0 || strncmp(sentence, "$GPGGA,", 7) == 0);
}

static bool sentenceIsRmc(const char *sentence)
{
  return (strncmp(sentence, "$GNRMC,", 7) == 0 || strncmp(sentence, "$GPRMC,", 7) == 0);
}

static void formatNmeaLatitude(double latDeg, char *nmeaLat, char *hemisphere)
{
  if (latDeg >= 0.0)
    *hemisphere = 'N';
  else
  {
    latDeg = -latDeg;
    *hemisphere = 'S';
  }

  int degrees = (int)latDeg;
  double minutes = (latDeg - degrees) * 60.0;
  sprintf(nmeaLat, "%02d%011.8f", degrees, minutes);
}

static void formatNmeaLongitude(double lonDeg, char *nmeaLon, char *hemisphere)
{
  if (lonDeg >= 0.0)
    *hemisphere = 'E';
  else
  {
    lonDeg = -lonDeg;
    *hemisphere = 'W';
  }

  int degrees = (int)lonDeg;
  double minutes = (lonDeg - degrees) * 60.0;
  sprintf(nmeaLon, "%03d%011.8f", degrees, minutes);
}

static uint8_t nmeaChecksum(const char *sentence)
{
  uint8_t crc = 0;
  for (int x = 1; sentence[x] != '\0' && sentence[x] != '*'; x++)
    crc ^= (uint8_t)sentence[x];
  return crc;
}

static bool replaceFieldFixed(char *sentence, int fieldIndex, const char *newValue)
{
  int currentField = 0;
  char *fieldStart = NULL;
  char *fieldEnd = NULL;

  for (char *p = sentence + 1; *p != '\0'; p++)
  {
    if (*p == ',' || *p == '*')
    {
      if (currentField == fieldIndex)
      {
        fieldEnd = p;
        break;
      }
      currentField++;
      fieldStart = p + 1;
    }
  }

  if (fieldStart == NULL || fieldEnd == NULL)
    return false;

  int fieldLen = fieldEnd - fieldStart;
  int newLen = strlen(newValue);
  if (newLen > fieldLen)
    return false;

  memcpy(fieldStart, newValue, newLen);
  for (int i = newLen; i < fieldLen; i++)
    fieldStart[i] = '0';

  return true;
}

static void updateSentenceChecksum(char *sentence)
{
  char *star = strchr(sentence, '*');
  if (star == NULL)
    return;

  *star = '\0';
  uint8_t crc = nmeaChecksum(sentence);
  sprintf(star, "*%02X", crc);
}

static bool patchGgaSentence(char *sentence)
{
  if (!terrainCompValid)
    return false;

  char nmeaLat[20];
  char nmeaLon[20];
  char latHem = 'N';
  char lonHem = 'E';
  char altField[20];

  formatNmeaLatitude(correctedLatitude, nmeaLat, &latHem);
  formatNmeaLongitude(correctedLongitude, nmeaLon, &lonHem);
  sprintf(altField, "%0.4f", correctedAltitude);

  char latHemStr[2];
  char lonHemStr[2];
  latHemStr[0] = latHem;
  latHemStr[1] = '\0';
  lonHemStr[0] = lonHem;
  lonHemStr[1] = '\0';

  replaceFieldFixed(sentence, 2, nmeaLat);
  replaceFieldFixed(sentence, 3, latHemStr);
  replaceFieldFixed(sentence, 4, nmeaLon);
  replaceFieldFixed(sentence, 5, lonHemStr);
  replaceFieldFixed(sentence, 9, altField);

  updateSentenceChecksum(sentence);
  return true;
}

static bool patchRmcSentence(char *sentence)
{
  if (!terrainCompValid)
    return false;

  char nmeaLat[20];
  char nmeaLon[20];
  char latHem = 'N';
  char lonHem = 'E';
  char latHemStr[2];
  char lonHemStr[2];

  formatNmeaLatitude(correctedLatitude, nmeaLat, &latHem);
  formatNmeaLongitude(correctedLongitude, nmeaLon, &lonHem);
  latHemStr[0] = latHem;
  latHemStr[1] = '\0';
  lonHemStr[0] = lonHem;
  lonHemStr[1] = '\0';

  replaceFieldFixed(sentence, 3, nmeaLat);
  replaceFieldFixed(sentence, 4, latHemStr);
  replaceFieldFixed(sentence, 5, nmeaLon);
  replaceFieldFixed(sentence, 6, lonHemStr);

  updateSentenceChecksum(sentence);
  return true;
}

bool patchNmeaSentence(char *sentence)
{
  if (!settings.enableTerrainComp || !online.bno086 || !terrainCompValid)
    return false;

  if (sentenceIsGga(sentence))
    return patchGgaSentence(sentence);

  if (sentenceIsRmc(sentence))
    return patchRmcSentence(sentence);

  return false;
}

void patchNmeaBuffer(uint8_t *buffer, size_t length)
{
  if (!settings.enableTerrainComp || !online.bno086 || !terrainCompValid)
    return;

  for (size_t i = 0; i < length; i++)
  {
    if (buffer[i] != '$')
      continue;

    size_t lineEnd = i;
    while (lineEnd < length && buffer[lineEnd] != '\n' && buffer[lineEnd] != '\r')
      lineEnd++;

    if (lineEnd <= i)
      continue;

    char sentence[100];
    size_t sentenceLen = lineEnd - i;
    if (sentenceLen >= sizeof(sentence))
      continue;

    memcpy(sentence, &buffer[i], sentenceLen);
    sentence[sentenceLen] = '\0';

    if (patchNmeaSentence(sentence) == true)
      memcpy(&buffer[i], sentence, sentenceLen);
  }
}

bool buildCorrectedGga(char *outBuffer, size_t outSize)
{
  if (outSize < 82 || !terrainCompValid)
    return false;

  char nmeaLat[20];
  char nmeaLon[20];
  char latHem = 'N';
  char lonHem = 'E';

  formatNmeaLatitude(correctedLatitude, nmeaLat, &latHem);
  formatNmeaLongitude(correctedLongitude, nmeaLon, &lonHem);

  char body[90];
  sprintf(body, "$GNGGA,%02d%02d%02d.%02d,%s,%c,%s,%c,%d,%02d,%.2f,%.4f,M,0.0,M,,",
          gnssHour, gnssMinute, gnssSecond, mseconds,
          nmeaLat, latHem, nmeaLon, lonHem,
          fixType, numSV, horizontalAccuracy, correctedAltitude);

  uint8_t crc = nmeaChecksum(body);
  snprintf(outBuffer, outSize, "%s*%02X\r\n", body, crc);
  return true;
}
