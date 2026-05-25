#ifdef COMPILE_TERRAIN_COMP

#include <SparkFun_BNO08x_Arduino_Library.h>
#include "TerrainComp.h"

BNO08x bno086;

TaskHandle_t bno086TaskHandle = NULL;

static void bno086SetReports()
{
  if (bno086.enableGameRotationVector(20) == true) // ~50 Hz
    log_d("BNO086 game rotation vector enabled");
  else
    log_d("BNO086 failed to enable game rotation vector");
}

static void gameQuatToEulerDeg(float i, float j, float k, float w, float &rollDeg, float &pitchDeg)
{
  float roll = atan2(2.0f * (w * i + j * k), 1.0f - 2.0f * (i * i + j * j));
  float pitch = asin(constrain(2.0f * (w * j - k * i), -1.0f, 1.0f));
  rollDeg = roll * 180.0f / PI;
  pitchDeg = pitch * 180.0f / PI;
}

void bno086Task(void *e)
{
  while (true)
  {
    if (online.bno086 == true && settings.enableTerrainComp == true)
    {
      if (takeI2cMutex(25) == true)
      {
        if (bno086.wasReset())
          bno086SetReports();

        if (bno086.getSensorEvent() == true)
        {
          if (bno086.getSensorEventID() == SENSOR_REPORTID_GAME_ROTATION_VECTOR)
          {
            float rollDeg = 0.0f;
            float pitchDeg = 0.0f;
            gameQuatToEulerDeg(
              bno086.getGameQuatI(),
              bno086.getGameQuatJ(),
              bno086.getGameQuatK(),
              bno086.getGameQuatReal(),
              rollDeg,
              pitchDeg);

            imuRollDeg = rollDeg;
            imuPitchDeg = pitchDeg;
          }
        }

        giveI2cMutex();
      }
    }

    delay(5);
    taskYIELD();
  }
}

void beginBNO086()
{
  online.bno086 = false;

  if (settings.enableTerrainComp == false)
    return;

  if (pin_bno086_int < 0 || pin_bno086_rst < 0)
  {
    Serial.println(F("BNO086 INT/RST pins not defined for this hardware variant"));
    return;
  }

  pinMode(pin_bno086_rst, OUTPUT);
  pinMode(pin_bno086_int, INPUT_PULLUP);

  if (takeI2cMutex(500) == false)
  {
    Serial.println(F("BNO086 init failed: I2C busy"));
    return;
  }

  if (bno086.begin(settings.bno086I2cAddress, Wire, pin_bno086_int, pin_bno086_rst) == false)
  {
    Serial.println(F("BNO086 not detected on I2C bus"));
    giveI2cMutex();
    return;
  }

  bno086SetReports();
  giveI2cMutex();

  online.bno086 = true;
  Serial.println(F("BNO086 online"));

  if (bno086TaskHandle == NULL)
  {
    xTaskCreate(
      bno086Task,
      "BNO086",
      4096,
      NULL,
      1,
      &bno086TaskHandle);
  }
}

#endif
