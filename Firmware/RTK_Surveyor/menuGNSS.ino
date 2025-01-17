//Configure the basic GNSS reception settings
//Update rate, constellations, etc
void menuGNSS()
{
  int menuTimeoutExtended = 30; //Increase time needed for complex data entry (mount point ID, caster credentials, etc).

  restartRover = false; //If user modifies any NTRIP settings, we need to restart the rover

  while (1)
  {
    Serial.println();
    Serial.println(F("Menu: GNSS Menu"));

    //Because we may be in base mode (always 1Hz), do not get freq from module, use settings instead
    float measurementFrequency = (1000.0 / settings.measurementRate) / settings.navigationRate;

    Serial.print(F("1) Set measurement rate in Hz: "));
    Serial.println(measurementFrequency, 4);

    Serial.print(F("2) Set measurement rate in seconds between measurements: "));
    Serial.println(1 / measurementFrequency, 2);

    Serial.print(F("3) Set dynamic model: "));
    switch (settings.dynamicModel)
    {
      case DYN_MODEL_PORTABLE:
        Serial.print(F("Portable"));
        break;
      case DYN_MODEL_STATIONARY:
        Serial.print(F("Stationary"));
        break;
      case DYN_MODEL_PEDESTRIAN:
        Serial.print(F("Pedestrian"));
        break;
      case DYN_MODEL_AUTOMOTIVE:
        Serial.print(F("Automotive"));
        break;
      case DYN_MODEL_SEA:
        Serial.print(F("Sea"));
        break;
      case DYN_MODEL_AIRBORNE1g:
        Serial.print(F("Airborne 1g"));
        break;
      case DYN_MODEL_AIRBORNE2g:
        Serial.print(F("Airborne 2g"));
        break;
      case DYN_MODEL_AIRBORNE4g:
        Serial.print(F("Airborne 4g"));
        break;
      case DYN_MODEL_WRIST:
        Serial.print(F("Wrist"));
        break;
      case DYN_MODEL_BIKE:
        Serial.print(F("Bike"));
        break;
      default:
        Serial.print(F("Unknown"));
        break;
    }
    Serial.println();

    Serial.println(F("4) Set Constellations "));

    Serial.print(F("5) Toggle NTRIP Client: "));
    if (settings.enableNtripClient == true) Serial.println(F("Enabled"));
    else Serial.println(F("Disabled"));

    if (settings.enableNtripClient == true)
    {
      Serial.print(F("6) Set WiFi SSID: "));
      Serial.println(settings.ntripClient_wifiSSID);

      Serial.print(F("7) Set WiFi PW: "));
      Serial.println(settings.ntripClient_wifiPW);

      Serial.print(F("8) Set Caster Address: "));
      Serial.println(settings.ntripClient_CasterHost);

      Serial.print(F("9) Set Caster Port: "));
      Serial.println(settings.ntripClient_CasterPort);

      Serial.print(F("10) Set Caster User Name: "));
      Serial.println(settings.ntripClient_CasterUser);

      Serial.print(F("11) Set Caster User Password: "));
      Serial.println(settings.ntripClient_CasterUserPW);

      Serial.print(F("12) Set Mountpoint: "));
      Serial.println(settings.ntripClient_MountPoint);

      Serial.print(F("13) Set Mountpoint PW: "));
      Serial.println(settings.ntripClient_MountPointPW);

      Serial.print(F("14) Toggle sending GGA Location to Caster: "));
      if (settings.ntripClient_TransmitGGA == true) Serial.println(F("Enabled"));
      else Serial.println(F("Disabled"));
    }

    Serial.println(F("x) Exit"));

    int incoming = getNumber(menuTimeout); //Timeout after x seconds

    if (incoming == 1)
    {
      Serial.print(F("Enter GNSS measurement rate in Hz: "));
      double rate = getDouble(menuTimeout); //Timeout after x seconds
      if (rate < 0.0 || rate > 20.0) //20Hz limit with all constellations enabled.
      {
        Serial.println(F("Error: measurement rate out of range"));
      }
      else
      {
        setMeasurementRates(1.0 / rate); //Convert Hz to seconds. This will set settings.measurementRate and settings.navigationRate
        //Settings recorded to NVM and file at main menu exit
      }
    }
    else if (incoming == 2)
    {
      Serial.print(F("Enter GNSS measurement rate in seconds between measurements: "));
      double rate = getDouble(menuTimeout); //Timeout after x seconds
      if (rate < 0.0 || rate > 8255.0) //Limit of 127 (navRate) * 65000ms (measRate) = 137 minute limit.
      {
        Serial.println(F("Error: measurement rate out of range"));
      }
      else
      {
        setMeasurementRates(rate); //This will set settings.measurementRate and settings.navigationRate
        //Settings recorded to NVM and file at main menu exit
      }
    }
    else if (incoming == 3)
    {
      Serial.println(F("Enter the dynamic model to use: "));
      Serial.println(F("1) Portable"));
      Serial.println(F("2) Stationary"));
      Serial.println(F("3) Pedestrian"));
      Serial.println(F("4) Automotive"));
      Serial.println(F("5) Sea"));
      Serial.println(F("6) Airborne 1g"));
      Serial.println(F("7) Airborne 2g"));
      Serial.println(F("8) Airborne 4g"));
      Serial.println(F("9) Wrist"));
      Serial.println(F("10) Bike"));

      int dynamicModel = getNumber(menuTimeout); //Timeout after x seconds
      if (dynamicModel < 1 || dynamicModel > DYN_MODEL_BIKE)
        Serial.println(F("Error: Dynamic model out of range"));
      else
      {
        if (dynamicModel == 1)
          settings.dynamicModel = DYN_MODEL_PORTABLE; //The enum starts at 0 and skips 1.
        else
          settings.dynamicModel = dynamicModel; //Recorded to NVM and file at main menu exit
      }
    }
    else if (incoming == 4)
    {
      menuConstellations();
    }
    else if (incoming == 5)
    {
      settings.enableNtripClient ^= 1;
      restartRover = true;
    }
    else if (incoming == 6 && settings.enableNtripClient == true)
    {
      Serial.print(F("Enter local WiFi SSID: "));
      readLine(settings.ntripClient_wifiSSID, sizeof(settings.ntripClient_wifiSSID), menuTimeoutExtended);
      restartRover = true;
    }
    else if (incoming == 7 && settings.enableNtripClient == true)
    {
      Serial.printf("Enter password for WiFi network %s: ", settings.ntripClient_wifiSSID);
      readLine(settings.ntripClient_wifiPW, sizeof(settings.ntripClient_wifiPW), menuTimeoutExtended);
      restartRover = true;
    }
    else if (incoming == 8 && settings.enableNtripClient == true)
    {
      Serial.print(F("Enter new Caster Address: "));
      readLine(settings.ntripClient_CasterHost, sizeof(settings.ntripClient_CasterHost), menuTimeoutExtended);
      restartRover = true;
    }
    else if (incoming == 9 && settings.enableNtripClient == true)
    {
      Serial.print(F("Enter new Caster Port: "));

      int ntripClient_CasterPort = getNumber(menuTimeoutExtended); //Timeout after x seconds
      if (ntripClient_CasterPort < 1 || ntripClient_CasterPort > 99999) //Arbitrary 99k max port #
        Serial.println(F("Error: Caster Port out of range"));
      else
        settings.ntripClient_CasterPort = ntripClient_CasterPort; //Recorded to NVM and file at main menu exit
      restartRover = true;
    }
    else if (incoming == 10 && settings.enableNtripClient == true)
    {
      Serial.printf("Enter user name for %s: ", settings.ntripClient_CasterHost);
      readLine(settings.ntripClient_CasterUser, sizeof(settings.ntripClient_CasterUser), menuTimeoutExtended);
      restartRover = true;
    }
    else if (incoming == 11 && settings.enableNtripClient == true)
    {
      Serial.printf("Enter user password for %s: ", settings.ntripClient_CasterHost);
      readLine(settings.ntripClient_CasterUserPW, sizeof(settings.ntripClient_CasterUserPW), menuTimeoutExtended);
      restartRover = true;
    }
    else if (incoming == 12 && settings.enableNtripClient == true)
    {
      Serial.print(F("Enter new Mount Point: "));
      readLine(settings.ntripClient_MountPoint, sizeof(settings.ntripClient_MountPoint), menuTimeoutExtended);
      restartRover = true;
    }
    else if (incoming == 13 && settings.enableNtripClient == true)
    {
      Serial.printf("Enter password for Mount Point %s: ", settings.ntripClient_MountPoint);
      readLine(settings.ntripClient_MountPointPW, sizeof(settings.ntripClient_MountPointPW), menuTimeoutExtended);
      restartRover = true;
    }
    else if (incoming == 14 && settings.enableNtripClient == true)
    {
      settings.ntripClient_TransmitGGA ^= 1;
      restartRover = true;
    }
    else if (incoming == STATUS_PRESSED_X)
      break;
    else if (incoming == STATUS_GETNUMBER_TIMEOUT)
      break;
    else
      printUnknown(incoming);
  }

  int maxWait = 2000;

  // Set dynamic model
  if (i2cGNSS.getDynamicModel(maxWait) != settings.dynamicModel)
  {
    if (i2cGNSS.setDynamicModel((dynModel)settings.dynamicModel, maxWait) == false)
      Serial.println(F("menuGNSS: setDynamicModel failed"));
  }

  while (Serial.available()) Serial.read(); //Empty buffer of any newline chars
}

//Controls the constellations that are used to generate a fix and logged
void menuConstellations()
{
  while (1)
  {
    Serial.println();
    Serial.println(F("Menu: Constellations Menu"));

    for (int x = 0 ; x < MAX_CONSTELLATIONS ; x++)
    {
      Serial.printf("%d) Constellation %s: ", x + 1, settings.ubxConstellations[x].textName);
      if (settings.ubxConstellations[x].enabled == true)
        Serial.print("Enabled");
      else
        Serial.print("Disabled");
      Serial.println();
    }

    Serial.println(F("x) Exit"));

    int incoming = getNumber(menuTimeout); //Timeout after x seconds

    if (incoming >= 1 && incoming <= MAX_CONSTELLATIONS)
    {
      incoming--; //Align choice to constallation array of 0 to 5

      settings.ubxConstellations[incoming].enabled ^= 1;

      //3.10.6: To avoid cross-correlation issues, it is recommended that GPS and QZSS are always both enabled or both disabled.
      if (incoming == SFE_UBLOX_GNSS_ID_GPS || incoming == 4) //QZSS ID is 5 but array location is 4
      {
        settings.ubxConstellations[SFE_UBLOX_GNSS_ID_GPS].enabled = settings.ubxConstellations[incoming].enabled; //GPS ID is 0 and array location is 0
        settings.ubxConstellations[4].enabled = settings.ubxConstellations[incoming].enabled; //QZSS ID is 5 but array location is 4
      }
    }
    else if (incoming == STATUS_PRESSED_X)
      break;
    else if (incoming == STATUS_GETNUMBER_TIMEOUT)
      break;
    else
      printUnknown(incoming);
  }

  //Apply current settings to module
  configureConstellations();

  while (Serial.available()) Serial.read(); //Empty buffer of any newline chars
}

//Given the number of seconds between desired solution reports, determine measurementRate and navigationRate
//measurementRate > 25 & <= 65535
//navigationRate >= 1 && <= 127
//We give preference to limiting a measurementRate to 30s or below due to reported problems with measRates above 30.
void setMeasurementRates(float secondsBetweenSolutions)
{
  uint16_t measRate = 0; //Calculate these locally and then attempt to apply them to ZED at completion
  uint16_t navRate = 0;

  //If we have more than an hour between readings, increase mesaurementRate to near max of 65,535
  if (secondsBetweenSolutions > 3600.0)
  {
    measRate = 65000;
  }

  //If we have more than 30s, but less than 3600s between readings, use 30s measurement rate
  else if (secondsBetweenSolutions > 30.0)
  {
    measRate = 30000;
  }

  //User wants measurements less than 30s (most common), set measRate to match user request
  //This will make navRate = 1.
  else
  {
    measRate = secondsBetweenSolutions * 1000.0;
  }

  navRate = secondsBetweenSolutions * 1000.0 / measRate; //Set navRate to nearest int value
  measRate = secondsBetweenSolutions * 1000.0 / navRate; //Adjust measurement rate to match actual navRate

  //Serial.printf("measurementRate / navRate: %d / %d\n\r", measRate, navRate);

  //If we successfully set rates, only then record to settings
  if (i2cGNSS.setMeasurementRate(measRate) == true && i2cGNSS.setNavigationRate(navRate) == true)
  {
    settings.measurementRate = measRate;
    settings.navigationRate = navRate;
  }
  else
  {
    Serial.println(F("menuGNSS: Failed to set measurement and navigation rates"));
  }
}

//We need to know our overall measurement frequency for things like setting the GSV NMEA sentence rate.
//This returns a float of the rate based on settings that is the readings per second (Hz).
float getMeasurementFrequency()
{
  uint16_t currentMeasurementRate = i2cGNSS.getMeasurementRate();
  uint16_t currentNavigationRate = i2cGNSS.getNavigationRate();

  currentNavigationRate = i2cGNSS.getNavigationRate();
  //The ZED-F9P will report an incorrect nav rate if we have rececently changed it.
  //Reading a second time insures a correct read.

  //Serial.printf("currentMeasurementRate / currentNavigationRate: %d / %d\n\r", currentMeasurementRate, currentNavigationRate);

  float measurementFrequency = (1000.0 / currentMeasurementRate) / currentNavigationRate;
  return (measurementFrequency);
}

//Updates the enabled constellations
bool configureConstellations()
{
  bool response = true;

  //If we have a corrupt constellation ID it can cause GNSS config to fail.
  //Reset to factory defaults.
  if (settings.ubxConstellations[0].gnssID == 255)
  {
    log_d("Constellation ID corrupt");
    factoryReset();
  }

  //long startTime = millis();
  for (int x = 0 ; x < MAX_CONSTELLATIONS ; x++)
  {
    //v1.12 ZED firmware does not allow for SBAS control
    //Also, if we can't identify the version (0), skip SBAS enable
    if (zedModuleType == PLATFORM_F9P && (zedFirmwareVersionInt == 112 || zedFirmwareVersionInt == 0) && x == 1) //SBAS
    {
      //Do nothing
    }
    else
    {
      //Standard UBX protocol method takes ~533-783ms
      uint8_t currentlyEnabled = getConstellation(settings.ubxConstellations[x].gnssID); //Qeury the module for the current setting
      if (currentlyEnabled != settings.ubxConstellations[x].enabled)
        response &= setConstellation(settings.ubxConstellations[x].gnssID, settings.ubxConstellations[x].enabled);
    }

    //Get/set val method takes ~642ms but does not work because we don't send additional sigCfg keys at same time
    //    uint8_t currentlyEnabled = i2cGNSS.getVal8(settings.ubxConstellations[x].configKey, VAL_LAYER_RAM, 1200);
    //    if (currentlyEnabled != settings.ubxConstellations[x].enabled)
    //      response &= i2cGNSS.setVal(settings.ubxConstellations[x].configKey, settings.ubxConstellations[x].enabled);
  }
  //long stopTime = millis();

  //Serial.printf("setConstellation time delta: %ld ms\n\r", stopTime - startTime);

  return (response);
}

//Print the module type and firmware version
void printModuleInfo()
{
  if (zedModuleType == PLATFORM_F9P)
    Serial.printf("ZED-F9P firmware: %s\n\r", zedFirmwareVersion);
  else if (zedModuleType == PLATFORM_F9R)
    Serial.printf("ZED-F9R firmware: %s\n\r", zedFirmwareVersion);
  else
    Serial.printf("Unknown module with firmware: %s\n\r", zedFirmwareVersion);
}
