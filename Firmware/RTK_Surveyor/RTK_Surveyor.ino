/*
  September 1st, 2020
  SparkFun Electronics
  Nathan Seidle

  This firmware runs the core of the SparkFun RTK Surveyor product. It runs on an ESP32
  and communicates with the ZED-F9P.

  Compiled with Arduino v1.8.15 with ESP32 core v2.0.1.
  v1.7 Moves to ESP32 core v2.0.0.

  Select the ESP32 Dev Module from the boards list. This maps the same pins to the ESP32-WROOM module.
  Select 'Minimal SPIFFS (1.9MB App)' from the partition list. This will enable SD firmware updates.

  Special thanks to Avinab Malla for guidance on getting xTasks implemented.

  The RTK Surveyor implements classic Bluetooth SPP to transfer data from the
  ZED-F9P to the phone and receive any RTCM from the phone and feed it back
  to the ZED-F9P to achieve RTK: F9PSerialWriteTask(), F9PSerialReadTask().

  A settings file is accessed on microSD if available otherwise settings are pulled from
  ESP32's file system LittleFS.

  As of v1.2, the heap is approximately 94072 during Rover Fix, 142260 during WiFi Casting. This is
  important to maintain as unit will begin to have stability issues at ~30k.

  The main loop handles lower priority updates such as:
    Fuel gauge checking and power LED color update
    Setup switch monitoring (module configure between Rover and Base)
    Text menu interactions

  Main Menu (Display MAC address / broadcast name):
    (Done) GNSS - Configure measurement rate, SBAS
    (Done) Log - Control messages logged to SD
    (Done) Broadcast - Control messages sent over BT SPP
    (Done) Base - Enter fixed coordinates, survey-in settings, WiFi/Caster settings,
    (Done) Ports - Configure Radio and Data port baud rates
    (Done) Test menu
    (Done) Firmware upgrade menu
    Enable various debug outputs sent over BT

  TODO:
    Add mountPointDownload and PWDownload to AP config
    Add casterTransmitGGA to AP config
    Add casterUser/PW to AP config
    Add maxLogLength_minutes to AP config

*/

const int FIRMWARE_VERSION_MAJOR = 1;
const int FIRMWARE_VERSION_MINOR = 11;

//#define COMPILE_WIFI //Comment out to remove all WiFi functionality
//#define COMPILE_BT //Comment out to disable all Bluetooth
#define ENABLE_DEVELOPER //Uncomment this line to enable special developer modes (don't check power button at startup)

//Define the RTK board identifier:
//  This is an int which is unique to this variant of the RTK Surveyor hardware which allows us
//  to make sure that the settings stored in flash (LittleFS) are correct for this version of the RTK
//  (sizeOfSettings is not necessarily unique and we want to avoid problems when swapping from one variant to another)
//  It is the sum of:
//    the major firmware version * 0x10
//    the minor firmware version
#define RTK_IDENTIFIER (FIRMWARE_VERSION_MAJOR * 0x10 + FIRMWARE_VERSION_MINOR)

#include "settings.h"

//Hardware connections
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//These pins are set in beginBoard()
int pin_batteryLevelLED_Red;
int pin_batteryLevelLED_Green;
int pin_positionAccuracyLED_1cm;
int pin_positionAccuracyLED_10cm;
int pin_positionAccuracyLED_100cm;
int pin_baseStatusLED;
int pin_bluetoothStatusLED;
int pin_microSD_CS;
int pin_zed_tx_ready;
int pin_zed_reset;
int pin_batteryLevel_alert;

int pin_muxA;
int pin_muxB;
int pin_powerSenseAndControl;
int pin_setupButton;
int pin_powerFastOff;
int pin_dac26;
int pin_adc39;
int pin_peripheralPowerControl;

int pin_radio_rx;
int pin_radio_tx;
int pin_radio_rst;
int pin_radio_pwr;
int pin_radio_cts;
int pin_radio_rts;

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//I2C for GNSS, battery gauge, display, accelerometer
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <Wire.h>
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//LittleFS for storing settings for different user profiles
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include "FS.h"
#include <LittleFS.h>
#define FORMAT_LITTLEFS_IF_FAILED true

const char *rtkProfileSettings = "SFERTK"; //Holds the profileNumber
const char *rtkSettings[] = {"SFERTK_0", "SFERTK_1", "SFERTK_2", "SFERTK_3"}; //User profiles
#define MAX_PROFILE_COUNT 4
uint8_t activeProfiles = 1;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//Handy library for setting ESP32 system time to GNSS time
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <ESP32Time.h> //http://librarymanager/All#ESP32Time
ESP32Time rtc;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//microSD Interface
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <SPI.h>
#include "SdFat.h" //http://librarymanager/All#sdfat_exfat by Bill Greiman. Currently uses v2.1.1

SdFat sd;

char platformFilePrefix[40] = "SFE_Surveyor"; //Sets the prefix for logs and settings files

SdFile ubxFile; //File that all gnss ubx messages setences are written to
unsigned long lastUBXLogSyncTime = 0; //Used to record to SD every half second
int startLogTime_minutes = 0; //Mark when we start any logging so we can stop logging after maxLogTime_minutes
int startCurrentLogTime_minutes = 0; //Mark when we start this specific log file so we can close it after x minutes and start a new one

SdFile newFirmwareFile; //File that is available if user uploads new firmware via web gui

//System crashes if two tasks access a file at the same time
//So we use a semaphore to see if file system is available
SemaphoreHandle_t xFATSemaphore;
const TickType_t fatSemaphore_shortWait_ms = 10 / portTICK_PERIOD_MS;
const TickType_t fatSemaphore_longWait_ms = 200 / portTICK_PERIOD_MS;

//Display used/free space in menu and config page
uint32_t sdCardSizeMB = 0;
uint32_t sdFreeSpaceMB = 0;
uint32_t sdUsedSpaceMB = 0;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//Connection settings to NTRIP Caster
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_WIFI
#include <WiFi.h>
#include "esp_wifi.h" //Needed for init/deinit of resources to free up RAM

WiFiClient caster;
#endif

unsigned long lastServerSent_ms = 0; //Time of last data pushed to caster
unsigned long lastServerReport_ms = 0; //Time of last report of caster bytes sent
int maxTimeBeforeHangup_ms = 10000; //If we fail to get a complete RTCM frame after 10s, then disconnect from caster

uint32_t casterBytesSent = 0; //Just a running total
uint32_t casterResponseWaitStartTime = 0; //Used to detect if caster service times out
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//GNSS configuration
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <SparkFun_u-blox_GNSS_Arduino_Library.h> //http://librarymanager/All#SparkFun_u-blox_GNSS

char zedFirmwareVersion[20]; //The string looks like 'HPG 1.12'. Output to debug menu and settings file.
uint8_t zedFirmwareVersionInt = 0; //Controls which features (constellations) can be configured (v1.12 doesn't support SBAS)
uint8_t zedModuleType = PLATFORM_F9P; //Controls which messages are supported and configured

// Extend the class for getModuleInfo. Used to diplay ZED-F9P firmware version in debug menu.
class SFE_UBLOX_GNSS_ADD : public SFE_UBLOX_GNSS
{
  public:
    boolean getModuleInfo(uint16_t maxWait = 1100); //Queries module, texts

    struct minfoStructure // Structure to hold the module info (uses 341 bytes of RAM)
    {
      char swVersion[30];
      char hwVersion[10];
      uint8_t extensionNo = 0;
      char extension[10][30];
    } minfo;
};

SFE_UBLOX_GNSS_ADD i2cGNSS;

//Used for config ZED for things not supported in library: getPortSettings, getSerialRate, getNMEASettings, getRTCMSettings
//This array holds the payload data bytes. Global so that we can use between config functions.
#ifdef MAX_PAYLOAD_SIZE
#undef MAX_PAYLOAD_SIZE
#define MAX_PAYLOAD_SIZE 384 // Override MAX_PAYLOAD_SIZE for getModuleInfo which can return up to 348 bytes
#endif
uint8_t settingPayload[MAX_PAYLOAD_SIZE];
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Battery fuel gauge and PWM LEDs
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library
SFE_MAX1704X lipo(MAX1704X_MAX17048);

// RTK Surveyor LED PWM properties
const int pwmFreq = 5000;
const int ledRedChannel = 0;
const int ledGreenChannel = 1;
const int ledBTChannel = 2;
const int pwmResolution = 8;

int pwmFadeAmount = 10;
int btFadeLevel = 0;

int battLevel = 0; //SOC measured from fuel gauge, in %. Used in multiple places (display, serial debug, log)
float battVoltage = 0.0;
float battChangeRate = 0.0;
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Hardware serial and BT buffers
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_BT
//We use a local copy of the BluetoothSerial library so that we can increase the RX buffer. See issue: https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/23
#include "src/BluetoothSerial/BluetoothSerial.h"
BluetoothSerial SerialBT;
#include "esp_bt.h" //Core access is needed for BT stop. See customBTstop() for more info.
#include "esp_gap_bt_api.h" //Needed for setting of pin. See issue: https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/5
#endif

char platformPrefix[40] = "Surveyor"; //Sets the prefix for broadcast names

HardwareSerial serialGNSS(2); //TX on 17, RX on 16

#define SERIAL_SIZE_RX (1024 * 6) //Should match buffer size in BluetoothSerial.cpp. Reduced from 16384 to make room for WiFi/NTRIP server capabilities
uint8_t rBuffer[SERIAL_SIZE_RX]; //Buffer for reading from F9P to SPP
TaskHandle_t F9PSerialReadTaskHandle = NULL; //Store handles so that we can kill them if user goes into WiFi NTRIP Server mode
const uint8_t F9PSerialReadTaskPriority = 1; //3 being the highest, and 0 being the lowest

#define SERIAL_SIZE_TX (1024 * 2)
uint8_t wBuffer[SERIAL_SIZE_TX]; //Buffer for writing from incoming SPP to F9P
TaskHandle_t F9PSerialWriteTaskHandle = NULL; //Store handles so that we can kill them if user goes into WiFi NTRIP Server mode
const uint8_t F9PSerialWriteTaskPriority = 1; //3 being the highest, and 0 being the lowest

TaskHandle_t pinUART2TaskHandle = NULL; //Dummy task to start UART2 on core 0.
volatile bool uart2pinned = false; //This variable is touched by core 0 but checked by core 1. Must be volatile.

//Reduced stack size from 10,000 to 2,000 to make room for WiFi/NTRIP server capabilities
const int readTaskStackSize = 2500;
const int writeTaskStackSize = 2000;

bool zedUartPassed = false; //Goes true during testing if ESP can communicate with ZED over UART
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//External Display
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <SparkFun_Qwiic_OLED.h> //http://librarymanager/All#SparkFun_Qwiic_Graphic_OLED
QwiicMicroOLED oled;

// Fonts
#include <res/qw_fnt_5x7.h>
#include <res/qw_fnt_8x16.h>

#include "icons.h"
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Firmware binaries loaded from SD
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <Update.h>
int binCount = 0;
const int maxBinFiles = 10;
char binFileNames[maxBinFiles][50];
const char* forceFirmwareFileName = "RTK_Surveyor_Firmware_Force.bin"; //File that will be loaded at startup regardless of user input
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Low frequency tasks
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <Ticker.h>

Ticker btLEDTask;
float btLEDTaskPace2Hz = 0.5;
float btLEDTaskPace33Hz = 0.03;
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Accelerometer for bubble leveling
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include "SparkFun_LIS2DH12.h" //Click here to get the library: http://librarymanager/All#SparkFun_LIS2DH12
SPARKFUN_LIS2DH12 accel;
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Buttons - Interrupt driven and debounce
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <JC_Button.h> // http://librarymanager/All#JC_Button
Button *setupBtn = NULL; //We can't instantiate the buttons here because we don't yet know what pin numbers to use
Button *powerBtn = NULL;

TaskHandle_t ButtonCheckTaskHandle = NULL;
const uint8_t ButtonCheckTaskPriority = 1; //3 being the highest, and 0 being the lowest
const int buttonTaskStackSize = 2000;

const int shutDownButtonTime = 2000; //ms press and hold before shutdown
unsigned long lastRockerSwitchChange = 0; //If quick toggle is detected (less than 500ms), enter WiFi AP Config mode
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//Webserver for serving config page from ESP32 as Acess Point
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_WIFI

#include "ESPAsyncWebServer.h" //Get from: https://github.com/me-no-dev/ESPAsyncWebServer 
#include "form.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
#endif

//Because the incoming string is longer than max len, there are multiple callbacks so we
//use a global to combine the incoming
char incomingSettings[3000];
int incomingSettingsSpot = 0;
unsigned long timeSinceLastIncomingSetting = 0;
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Global variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
uint8_t unitMACAddress[6]; //Use MAC address in BT broadcast and display
char deviceName[20]; //The serial string that is broadcast. Ex: 'Surveyor Base-BC61'
const byte menuTimeout = 15; //Menus will exit/timeout after this number of seconds
int systemTime_minutes = 0; //Used to test if logging is less than max minutes
uint32_t powerPressedStartTime = 0; //Times how long user has been holding power button, used for power down
uint8_t debounceDelay = 20; //ms to delay between button reads

uint32_t lastBattUpdate = 0;
uint32_t lastDisplayUpdate = 0;
bool forceDisplayUpdate = false; //Goes true when setup is pressed, causes display to refresh real time
uint32_t lastSystemStateUpdate = 0;
bool forceSystemStateUpdate = false; //Set true to avoid update wait
uint32_t lastAccuracyLEDUpdate = 0;
uint32_t lastBaseLEDupdate = 0; //Controls the blinking of the Base LED

uint32_t lastFileReport = 0; //When logging, print file record stats every few seconds
long lastStackReport = 0; //Controls the report rate of stack highwater mark within a task
uint32_t lastHeapReport = 0; //Report heap every 1s if option enabled
uint32_t lastTaskHeapReport = 0; //Report task heap every 1s if option enabled
uint32_t lastCasterLEDupdate = 0; //Controls the cycling of position LEDs during casting
uint32_t lastRTCAttempt = 0; //Wait 1000ms between checking GNSS for current date/time

uint32_t lastSatelliteDishIconUpdate = 0;
bool satelliteDishIconDisplayed = false; //Toggles as lastSatelliteDishIconUpdate goes above 1000ms
uint32_t lastCrosshairIconUpdate = 0;
bool crosshairIconDisplayed = false; //Toggles as lastCrosshairIconUpdate goes above 1000ms
uint32_t lastBaseIconUpdate = 0;
bool baseIconDisplayed = false; //Toggles as lastBaseIconUpdate goes above 1000ms
uint32_t lastWifiIconUpdate = 0;
bool wifiIconDisplayed = false; //Toggles as lastWifiIconUpdate goes above 1000ms
uint32_t lastLoggingIconUpdate = 0;
int loggingIconDisplayed = 0; //Increases every 500ms while logging

uint64_t lastLogSize = 0;
bool logIncreasing = false; //Goes true when log file is greater than lastLogSize
bool reuseLastLog = false; //Goes true if we have a reset due to software (rather than POR)

uint32_t lastRTCMPacketSent = 0; //Used to count RTCM packets sent during base mode
uint32_t rtcmPacketsSent = 0; //Used to count RTCM packets sent via processRTCM()

uint32_t maxSurveyInWait_s = 60L * 15L; //Re-start survey-in after X seconds

uint32_t totalWriteTime = 0; //Used to calculate overall write speed using SdFat library

bool setupByPowerButton = false; //We can change setup via tapping power button

uint16_t svinObservationTime = 0; //Use globals so we don't have to request these values multiple times (slow response)
float svinMeanAccuracy = 0;

uint32_t lastSetupMenuChange = 0; //Auto-selects the setup menu option after 1500ms
uint32_t lastTestMenuChange = 0; //Avoids exiting the test menu for at least 1 second

bool firstRoverStart = false; //Used to detect if user is toggling power button at POR to enter test menu

bool newEventToRecord = false; //Goes true when INT pin goes high
uint32_t triggerCount = 0; //Global copy - TM2 event counter
uint32_t towMsR = 0; //Global copy - Time Of Week of rising edge (ms)
uint32_t towSubMsR = 0; //Global copy - Millisecond fraction of Time Of Week of rising edge in nanoseconds

long lastReceivedRTCM_ms = 0;       //5 RTCM messages take approximately ~300ms to arrive at 115200bps
int timeBetweenGGAUpdate_ms = 10000; //GGA is required for Rev2 NTRIP casters. Don't transmit but once every 10 seconds
long lastTransmittedGGA_ms = 0;

//Used for GGA sentence parsing from incoming NMEA
bool ggaSentenceStarted = false;
bool ggaSentenceComplete = false;
bool ggaTransmitComplete = false; //Goes true once we transmit GGA to the caster
char ggaSentence[128] = {0};
byte ggaSentenceSpot = 0;
int ggaSentenceEndSpot = 0;

bool newAPSettings = false; //Goes true when new setting is received via AP config. Allows us to record settings when combined with a reset.

unsigned int binBytesSent = 0; //Tracks firmware bytes sent over WiFi OTA update via AP config.
int binBytesLastUpdate = 0; //Allows websocket notification to be sent every 100k bytes

unsigned long startTime = 0; //Used for checking longest running functions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void setup()
{
  Serial.begin(115200); //UART0 for programming and debugging

  Wire.begin(); //Start I2C on core 1
  Wire.setClock(400000);

  beginGNSS(); //Connect to GNSS to get module type

  beginFS(); //Start file system for settings

  beginBoard(); //Determine what hardware platform we are running on and check on button

  beginDisplay(); //Start display first to be able to display any errors

  beginLEDs(); //LED and PWM setup

  beginSD(); //Test if SD is present

  loadSettings(); //Attempt to load settings after SD is started so we can read the settings file if available

  beginUART2(); //Start UART2 on core 0, used to receive serial from ZED and pass out over SPP

  beginFuelGauge(); //Configure battery fuel guage monitor
  checkBatteryLevels(); //Force check so you see battery level immediately at power on

  configureGNSS(); //Configure ZED module

  beginAccelerometer();

  beginExternalTriggers(); //Configure the time pulse output and TM2 input

  beginSystemState(); //Determine initial system state. Start task for button monitoring.

  Serial.flush(); //Complete any previous prints

  danceLEDs(); //Turn on LEDs like a car dashboard
}

void loop()
{
  if (online.gnss == true)
  {
    i2cGNSS.checkUblox(); //Regularly poll to get latest data and any RTCM
    i2cGNSS.checkCallbacks(); //Process any callbacks: ie, eventTriggerReceived
  }

  updateSystemState();

  updateBattLEDs();

  updateDisplay();

  updateRTC(); //Set system time to GNSS once we have fix

  updateLogs(); //Record any new data. Create or close files as needed.

  reportHeap(); //If debug enabled, report free heap

  //Menu system via ESP32 USB connection
  if (Serial.available()) menuMain(); //Present user menu

  //Convert current system time to minutes. This is used in F9PSerialReadTask()/updateLogs() to see if we are within max log window.
  systemTime_minutes = millis() / 1000L / 60;

  delay(10); //A small delay prevents panic if no other I2C or functions are called
}

//Create or close files as needed (startup or as user changes settings)
//Push new data to log as needed
void updateLogs()
{
  if (online.logging == false && settings.enableLogging == true)
  {
    beginLogging();
  }
  else if (online.logging == true && settings.enableLogging == false)
  {
    //Close down file
    if (xSemaphoreTake(xFATSemaphore, fatSemaphore_longWait_ms) == pdPASS)
    {
      ubxFile.sync();
      ubxFile.close();
      online.logging = false;
      xSemaphoreGive(xFATSemaphore); //Release semaphore
    }
  }
  else if (online.logging == true && settings.enableLogging == true && (systemTime_minutes - startCurrentLogTime_minutes) >= settings.maxLogLength_minutes)
  {
    //Close down file. A new one will be created at the next calling of updateLogs().
    if (xSemaphoreTake(xFATSemaphore, fatSemaphore_longWait_ms) == pdPASS)
    {
      ubxFile.sync();
      ubxFile.close();
      online.logging = false;
      xSemaphoreGive(xFATSemaphore); //Release semaphore
    }
  }

  if (online.logging == true)
  {
    //Force file sync every 5000ms
    if (millis() - lastUBXLogSyncTime > 5000)
    {
      if (xSemaphoreTake(xFATSemaphore, fatSemaphore_shortWait_ms) == pdPASS)
      {
        if (productVariant == RTK_SURVEYOR)
          digitalWrite(pin_baseStatusLED, !digitalRead(pin_baseStatusLED)); //Blink LED to indicate logging activity

        long startWriteTime = micros();
        ubxFile.sync();
        long stopWriteTime = micros();
        totalWriteTime += stopWriteTime - startWriteTime; //Used to calculate overall write speed

        if (productVariant == RTK_SURVEYOR)
          digitalWrite(pin_baseStatusLED, !digitalRead(pin_baseStatusLED)); //Return LED to previous state

        updateDataFileAccess(&ubxFile); // Update the file access time & date

        lastUBXLogSyncTime = millis();
        xSemaphoreGive(xFATSemaphore);
      } //End xFATSemaphore
      else
      {
        log_d("Semaphore failed to yield");
      }
    }

    //Record any pending trigger events
    if (newEventToRecord == true)
    {
      Serial.println("Recording event");

      //Record trigger count with Time Of Week of rising edge (ms) and Millisecond fraction of Time Of Week of rising edge (ns)
      char eventData[82]; //Max NMEA sentence length is 82
      snprintf(eventData, sizeof(eventData), "%d,%d,%d", triggerCount, towMsR, towSubMsR);

      char nmeaMessage[82]; //Max NMEA sentence length is 82
      createNMEASentence(CUSTOM_NMEA_TYPE_EVENT, nmeaMessage, eventData); //textID, buffer, text

      if (xSemaphoreTake(xFATSemaphore, fatSemaphore_shortWait_ms) == pdPASS)
      {
        ubxFile.println(nmeaMessage);

        xSemaphoreGive(xFATSemaphore);
        newEventToRecord = false;
      }
    }

    //Report file sizes to show recording is working
    if (millis() - lastFileReport > 5000)
    {
      long fileSize = 0;

      //Attempt to access file system. This avoids collisions with file writing from other functions like recordSystemSettingsToFile() and F9PSerialReadTask()
      if (xSemaphoreTake(xFATSemaphore, fatSemaphore_shortWait_ms) == pdPASS)
      {
        fileSize = ubxFile.fileSize();

        xSemaphoreGive(xFATSemaphore);
      }

      if (fileSize > 0)
      {
        lastFileReport = millis();
        Serial.printf("UBX file size: %ld", fileSize);

        if ((systemTime_minutes - startLogTime_minutes) < settings.maxLogTime_minutes)
        {
          //Calculate generation and write speeds every 5 seconds
          uint32_t fileSizeDelta = fileSize - lastLogSize;
          Serial.printf(" - Generation rate: %0.1fkB/s", fileSizeDelta / 5.0 / 1000.0);

          if (totalWriteTime > 0)
            Serial.printf(" - Write speed: %0.1fkB/s", fileSizeDelta / (totalWriteTime / 1000000.0) / 1000.0);
          else
            Serial.printf(" - Write speed: 0.0kB/s");
        }
        else
        {
          Serial.printf(" reached max log time %d", settings.maxLogTime_minutes);
        }

        Serial.println();

        totalWriteTime = 0; //Reset write time every 5s

        if (fileSize > lastLogSize)
        {
          lastLogSize = fileSize;
          logIncreasing = true;
        }
        else
        {
          log_d("No increase in file size");
          logIncreasing = false;
        }
      }
    }
  }
}

//Once we have a fix, sync system clock to GNSS
//All SD writes will use the system date/time
void updateRTC()
{
  if (online.rtc == false)
  {
    if (online.gnss == true)
    {
      if (millis() - lastRTCAttempt > 1000)
      {
        lastRTCAttempt = millis();

        i2cGNSS.checkUblox();

        bool timeValid = false;
        if (i2cGNSS.getTimeValid() == true && i2cGNSS.getDateValid() == true) //Will pass if ZED's RTC is reporting (regardless of GNSS fix)
          timeValid = true;
        if (i2cGNSS.getConfirmedTime() == true && i2cGNSS.getConfirmedDate() == true) //Requires GNSS fix
          timeValid = true;

        if (timeValid == true)
        {
          //Set the internal system time
          //This is normally set with WiFi NTP but we will rarely have WiFi
          rtc.setTime(i2cGNSS.getSecond(), i2cGNSS.getMinute(), i2cGNSS.getHour(), i2cGNSS.getDay(), i2cGNSS.getMonth(), i2cGNSS.getYear());  // 17th Jan 2021 15:24:30

          online.rtc = true;

          Serial.print(F("System time set to: "));
          Serial.println(rtc.getDateTime(true));

          recordSystemSettingsToFile(); //This will re-record the setting file with current date/time.
        }
        else
        {
          Serial.println("No GNSS date/time available for system RTC.");
        } //End timeValid
      } //End lastRTCAttempt
    } //End online.gnss
  } //End online.rtc
}

void printElapsedTime(const char* title)
{
  Serial.printf("%s: %d\n\r", title, millis() - startTime);
}
