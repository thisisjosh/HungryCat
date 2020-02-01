//"RBL_nRF8001.h/spi.h/boards.h" is needed in every new project
#include <SPI.h>
#include <EEPROM.h>
#include <boards.h>
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
const int MAX_ALARMS = 8;
int speed = 10;
int sliceSteps = 255;
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

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    out(String(datestring) + '\n');
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
      msg += " on";
    }
    else
    {
      msg += " off";
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
  out(String("Set alarm ") + i);
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
      myStepper.step(sliceSteps);
      delay(500);
      lowStepper();
      lastAlarmTriggered = now;
      break;
    }
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

  if(strcmp(buf, "reset") == 0)
  {
    resetAlarms();
  }
  else if(strcmp(buf, "list") == 0)
  {
    listAlarms();
  }
  else if(strcmp(buf, "now") == 0)
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
  else if( ms.Match("remove (%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    slot = atoi(bufMatch);
    removeAlarm(slot);
  }
  else if( ms.Match("l(%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    steps = atoi(bufMatch);
    out(String("Stepping left ") + steps);
    myStepper.step(steps);
    delay(500);
    lowStepper();
  }
  else if( ms.Match("r(%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    steps = -1 * atoi(bufMatch);
    out(String("Stepping right ") + steps);
    myStepper.step(steps);
    delay(500);
    lowStepper();
  }
  else if( ms.Match("s(%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    speed = atoi(bufMatch);
    myStepper.setSpeed(speed);
  }
  else if( ms.Match("(%d+) (%d+):(%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture (bufMatch, 0);
    slot = atoi(bufMatch);
    
    ms.GetCapture (bufMatch, 1);
    t.hour = atoi(bufMatch);
    
    ms.GetCapture (bufMatch, 2);
    t.minute = atoi(bufMatch);

    t.isActive = true;
    setAlarm(slot, t);
  }
  else if( ms.Match("now (.+),(%d+:%d+:%d+)", 0) == REGEXP_MATCHED)
  {
    ms.GetCapture(bufMatch, 0);
    ms.GetCapture(bufMatch2, 1);
    RtcDateTime newTime = RtcDateTime(bufMatch, bufMatch2);
    Rtc.SetDateTime(newTime);
  }
  else
  {
    String msg = String("unknown command: ") + buf;
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

