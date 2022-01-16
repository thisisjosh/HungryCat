// https://github.com/thisisjosh/HungryCat

//"RBL_nRF8001.h/spi.h/boards.h" is needed in every new project
// follow https://github.com/RedBearLab/BLEShield/blob/master/Docs/LibraryManager.pdf
// use "no new line" when sending commands using the Serial Monitor window
#include <SPI.h>
#include <EEPROM.h>
#include <Boards.h>
#include <RBL_nRF8001.h>
#include <Regexp.h> // https://www.arduinolibraries.info/libraries/regexp
#include <Stepper.h>
#include <ThreeWire.h> 
#include <RtcDS1302.h> // https://github.com/Makuna/Rtc

struct SimpleAlarm {
  bool isActive;
  int hour, minute;
};

// ULN2003 Driver Board Module + 28BYJ-48
const int stepsPerRevolution = 2038; // https://www.seeedstudio.com/blog/2019/03/04/driving-a-28byj-48-stepper-motor-with-a-uln2003-driver-board-and-arduino/
Stepper myStepper(stepsPerRevolution, 2, 4, 3, 5); // https://forum.arduino.cc/index.php?topic=143276.0
const int BUF_SIZE = 32;
const int MAX_ALARMS = 4;
int speed = 10;
int sliceSteps = 255;
int alarmStepsLeft = 0;
int alarmStepsRight = 0;
SimpleAlarm alarms[MAX_ALARMS];
RtcDateTime lastAlarmTriggered;

// DS1302 DAT/IO - orange 7
// DS1302 CLK/SCLK - brown 6
// DS1302 RST/CE - yellow 10
ThreeWire myWire(7,6,10); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    // This should be changed to UTC. Using PST to make this easy on me.
    // Use a format that js likes 2020-02-02T10:52:37
    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%04u-%02u-%02uT%02u:%02u:%02u"),
            dt.Year(),
            dt.Month(),
            dt.Day(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    out(String(datestring));
    out(String('\n'));
}

void setupRtc()
{
    Serial.print("compiled: ");
    Serial.print(__DATE__);
    Serial.println(__TIME__);

    Rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    printDateTime(compiled);
    Serial.println();

    if (!Rtc.IsDateTimeValid()) 
    {
        // Common Causes:
        //    1) first time you ran and the device wasn't running yet
        //    2) the battery on the device is low or even missing

        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
    }

    if (Rtc.GetIsWriteProtected())
    {
        Serial.println("RTC was write protected, enabling writing now");
        Rtc.SetIsWriteProtected(false);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) 
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }
    else if (now > compiled) 
    {
        Serial.println("RTC is newer than compile time. (this is expected)");
    }
    else if (now == compiled) 
    {
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }
}

void setup()
{  
  // Default pins set to 9 and 8 for REQN and RDYN
  // Set your REQN and RDYN here before ble_begin() if you need
  //ble_set_pins(3, 2);
  
  // Set your BLE Shield name here, max. length 10
  char bleName[] = "HungryCat";
  ble_set_name(bleName);
  
  // Init. and start BLE library.
  ble_begin();
  
  // Enable serial debug
  Serial.begin(57600);

  // Load saved alarms from disk
  for(int i = 0; i < MAX_ALARMS; i++)
  {
    EEPROM.get(i * sizeof(SimpleAlarm), alarms[i]);
  }

  EEPROM.get(MAX_ALARMS * sizeof(SimpleAlarm), alarmStepsLeft);
  EEPROM.get(MAX_ALARMS * sizeof(SimpleAlarm)+sizeof(int), alarmStepsRight);
 
  myStepper.setSpeed(speed);
  setupRtc();

  lastAlarmTriggered = Rtc.GetDateTime();

  // pinMode(7, OUTPUT);
}

void lowStepper()
{
  digitalWrite(2, LOW);
  digitalWrite(3, LOW);
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
}

void out(const String& str)
{
  char buf[BUF_SIZE];
  str.toCharArray(buf, BUF_SIZE);
  bleWriteStr(buf);
}

void bleWriteStr(const char* str)
{
  int i = 0;
  while(str[i] != '\0')
  {
    Serial.write(str[i]);
    ble_write(str[i]);
    i++;
  }
  Serial.write('\n');
  ble_write('\n');
  ble_do_events();
}

void resetAlarms()
{
  for(int i = 0; i < MAX_ALARMS; i++)
  {
    alarms[i].isActive = false;
    alarms[i].hour = 0;
    alarms[i].minute = 0;
    EEPROM.put(i * sizeof(SimpleAlarm), alarms[i]); // save to disk
  }

  out(String("All alarms reset"));
}

void listAlarms()
{
  for(int i = 0; i < MAX_ALARMS; i++)
  {
    String msg = String("alarm ") + i + " " + alarms[i].hour + ":" + alarms[i].minute;
    if(alarms[i].isActive)
    {
      msg += " 1";
    }
    else
    {
      msg += " 0";
    }
    out(msg);
  }
}

int findAlarm(SimpleAlarm t)
{
  for(int i = 0; i < MAX_ALARMS; i++)
  {
    if(alarms[i].hour == t.hour && alarms[i].minute == t.minute)
      return i;
  }

  return -1;
}

void setAlarm(int i, SimpleAlarm t)
{
  alarms[i] = t;
  EEPROM.put(i * sizeof(SimpleAlarm), alarms[i]); // save to disk
  out(String("Set alarm ") + i + " " + alarms[i].hour + ":" + alarms[i].minute + " " + alarms[i].isActive);
}

void removeAlarm(int i)
{
  alarms[i].isActive = false;
  alarms[i].hour = 0;
  alarms[i].minute = 0;
  EEPROM.put(i * sizeof(SimpleAlarm), alarms[i]);
  out(String("Removed alarm ") + i);
}

void doAlarms()
{
  RtcDateTime now = Rtc.GetDateTime();

  for(int i = 0; i < MAX_ALARMS; i++)
  {
    if(now.TotalSeconds() <= (lastAlarmTriggered.TotalSeconds() + 61) )
    {
      break; // Only trigger an alarm once per minnute
    }
    
    if(alarms[i].isActive == true && alarms[i].hour == now.Hour() && alarms[i].minute == now.Minute())
    {
      out(String("Alarm Triggered ") + i);
      myStepper.step(alarmStepsLeft);
      delay(500);
      lowStepper();
      myStepper.step(alarmStepsRight);
      delay(500);
      lowStepper();
      lastAlarmTriggered = now;
      break;
    }
  }
}

void showCurrentTime()
{
  RtcDateTime now = Rtc.GetDateTime();

  printDateTime(now);
  Serial.println();

  if (!now.IsValid())
  {
      // Common Causes:
      //    1) the battery on the device is low or even missing and the power line was disconnected
      Serial.println("RTC lost confidence in the DateTime!");
  }
}

void handleInput(char* buf)
{
  char bufMatch[16];
  char bufMatch2[16];
  MatchState ms;
  ms.Target(buf);  // what to search
  SimpleAlarm t;
  int slot;
  int steps;
  uint32_t timeNow;

  if(strcmp(buf, "reset") == 0)
  {
    resetAlarms();
  }
  else if(strcmp(buf, "list") == 0)
  {
    listAlarms();
    showCurrentTime();
    out(String("alarmStepsLeft:") + alarmStepsLeft);
    out(String("alarmStepsRight:") + alarmStepsRight);
  }
  else if(strcmp(buf, "now") == 0)
  {
    showCurrentTime();
  }
  else if( ms.Match("remove (%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    slot = atoi(bufMatch);
    removeAlarm(slot);
  }
  else if( ms.Match("left (%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    steps = atoi(bufMatch);
    out(String("Stepping left ") + steps);
    myStepper.step(steps);
    delay(500);
    lowStepper();
  }
  else if( ms.Match("right (%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    steps = -1 * atoi(bufMatch);
    out(String("Stepping right ") + steps);
    myStepper.step(steps);
    delay(500);
    lowStepper();
  }
  else if( ms.Match("leftset (%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    alarmStepsLeft = atoi(bufMatch);
    out(String("Set left ") + alarmStepsLeft);
    EEPROM.put(MAX_ALARMS * sizeof(SimpleAlarm), alarmStepsLeft);
  }
  else if( ms.Match("rightset (%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    alarmStepsRight = -1 * atoi(bufMatch);
    out(String("Set right ") + alarmStepsRight);
    EEPROM.put(MAX_ALARMS * sizeof(SimpleAlarm)+sizeof(int), alarmStepsRight);
  }
  else if( ms.Match("s(%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    speed = atoi(bufMatch);
    myStepper.setSpeed(speed);
  }
  else if( ms.Match("(%d+) (%d+):(%d+) (%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture (bufMatch, 0);
    slot = atoi(bufMatch);
    
    ms.GetCapture (bufMatch, 1);
    t.hour = atoi(bufMatch);
    
    ms.GetCapture (bufMatch, 2);
    t.minute = atoi(bufMatch);

    ms.GetCapture (bufMatch, 3);
    t.isActive = atoi(bufMatch);

    setAlarm(slot, t);
  }
  else if( ms.Match("now (%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    timeNow = strtoul(bufMatch,NULL,10);
    RtcDateTime newTime = RtcDateTime(timeNow);
    Rtc.SetDateTime(newTime);
    out(String("clock updated to ") + " " + timeNow);
  }
  else
  {
    String msg = String("unknown command: <") + buf + String(">");
    out(msg);
  }
}

void loop()
{
  char buf[BUF_SIZE] = {0};
  unsigned char len = 0;
  String line;

  if( ble_available() )
  {
    while ( ble_available() )
    {
      buf[len++] = ble_read();
    }
    buf[len] = '\0';

    handleInput(buf);
  }

  if ( Serial.available() )
  {
    line = Serial.readString();
    line.toCharArray(buf, BUF_SIZE);

    handleInput(buf);
  }

  ble_do_events();
  doAlarms();
 // digitalWrite(activeLed, HIGH);
  delay(500);
}
