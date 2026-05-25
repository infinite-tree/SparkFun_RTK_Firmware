#ifdef COMPILE_TERRAIN_COMP

void menuTerrain()
{
  while (1)
  {
    Serial.println();
    Serial.println(F("Menu: Terrain Compensation"));

    Serial.print(F("BNO086: "));
    if (online.bno086 == true) Serial.println(F("Online"));
    else Serial.println(F("Offline"));

    Serial.print(F("1) Enable Terrain Compensation: "));
    if (settings.enableTerrainComp == true) Serial.println(F("Enabled"));
    else Serial.println(F("Disabled"));

    Serial.print(F("2) Pole Height (m): "));
    Serial.println(settings.poleHeightM, 3);

    Serial.print(F("3) Attitude Filter (Hz): "));
    Serial.println(settings.terrainAttitudeFilterHz, 2);

    Serial.print(F("4) Position Smooth Tau (s): "));
    Serial.println(settings.terrainPositionSmoothTauS, 2);

    Serial.print(F("5) Roll Offset (deg): "));
    Serial.println(settings.terrainRollOffsetDeg, 2);

    Serial.print(F("6) Pitch Offset (deg): "));
    Serial.println(settings.terrainPitchOffsetDeg, 2);

    Serial.print(F("7) BNO086 I2C Address: 0x"));
    Serial.println(settings.bno086I2cAddress, HEX);

    Serial.println(F("z) Level Zero (capture offsets on flat ground)"));

    Serial.println(F("x) Exit"));

    byte incoming = getByteChoice(menuTimeout);

    if (incoming == '1')
    {
      settings.enableTerrainComp ^= 1;
    }
    else if (incoming == '2')
    {
      Serial.print(F("Enter pole height in meters (0.1 to 10): "));
      float height = getDouble(menuTimeout);
      if (height >= 0.1 && height <= 10.0)
        settings.poleHeightM = height;
      else
        Serial.println(F("Error: Pole height out of range"));
    }
    else if (incoming == '3')
    {
      Serial.print(F("Enter attitude filter in Hz (0.1 to 10): "));
      float hz = getDouble(menuTimeout);
      if (hz >= 0.1 && hz <= 10.0)
        settings.terrainAttitudeFilterHz = hz;
      else
        Serial.println(F("Error: Filter value out of range"));
    }
    else if (incoming == '4')
    {
      Serial.print(F("Enter position smooth tau in seconds (0 to 5): "));
      float tau = getDouble(menuTimeout);
      if (tau >= 0.0 && tau <= 5.0)
        settings.terrainPositionSmoothTauS = tau;
      else
        Serial.println(F("Error: Tau out of range"));
    }
    else if (incoming == '5')
    {
      Serial.print(F("Enter roll offset in degrees (-45 to 45): "));
      float offset = getDouble(menuTimeout);
      if (offset >= -45.0 && offset <= 45.0)
        settings.terrainRollOffsetDeg = offset;
      else
        Serial.println(F("Error: Offset out of range"));
    }
    else if (incoming == '6')
    {
      Serial.print(F("Enter pitch offset in degrees (-45 to 45): "));
      float offset = getDouble(menuTimeout);
      if (offset >= -45.0 && offset <= 45.0)
        settings.terrainPitchOffsetDeg = offset;
      else
        Serial.println(F("Error: Offset out of range"));
    }
    else if (incoming == '7')
    {
      Serial.print(F("Enter I2C address in hex (4A or 4B): "));
      uint16_t addr = getNumber(menuTimeout);
      if (addr == 0x4A || addr == 0x4B)
        settings.bno086I2cAddress = (uint8_t)addr;
      else
        Serial.println(F("Error: Address must be 0x4A or 0x4B"));
    }
    else if (incoming == 'z')
    {
      terrainCompLevelZero();
    }
    else if (incoming == 'x' || incoming == STATUS_GETBYTE_TIMEOUT)
      break;
    else
      printUnknown(incoming);
  }

  while (Serial.available()) Serial.read();
}

#endif
