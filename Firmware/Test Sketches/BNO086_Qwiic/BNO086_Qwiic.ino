/*
  BNO086 Qwiic bring-up test for SparkFun RTK products.

  Connect SparkFun VR IMU Breakout BNO086 via Qwiic plus INT/RST to GPIO 18/19.
  Open serial monitor at 115200 baud.
*/

#include <Wire.h>
#include <SparkFun_BNO08x_Arduino_Library.h>

#define BNO08X_INT 18
#define BNO08X_RST 19
#define BNO08X_ADDR 0x4B

BNO08x myIMU;

void setReports()
{
  if (myIMU.enableGameRotationVector(20) == true)
    Serial.println(F("Game rotation vector enabled"));
  else
    Serial.println(F("Failed to enable game rotation vector"));
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  Wire.begin();

  if (myIMU.begin(BNO08X_ADDR, Wire, BNO08X_INT, BNO08X_RST) == false)
  {
    Serial.println(F("BNO086 not detected. Check Qwiic wiring and INT/RST pins."));
    while (1)
      delay(100);
  }

  Serial.println(F("BNO086 online"));
  setReports();
}

void loop()
{
  if (myIMU.wasReset())
    setReports();

  if (myIMU.getSensorEvent() == true)
  {
    if (myIMU.getSensorEventID() == SENSOR_REPORTID_GAME_ROTATION_VECTOR)
    {
      float i = myIMU.getGameQuatI();
      float j = myIMU.getGameQuatJ();
      float k = myIMU.getGameQuatK();
      float w = myIMU.getGameQuatReal();

      float roll = atan2(2.0f * (w * i + j * k), 1.0f - 2.0f * (i * i + j * j)) * 180.0f / PI;
      float pitch = asin(constrain(2.0f * (w * j - k * i), -1.0f, 1.0f)) * 180.0f / PI;

      Serial.print(F("Roll: "));
      Serial.print(roll, 2);
      Serial.print(F(" Pitch: "));
      Serial.println(pitch, 2);
    }
  }

  delay(5);
}
