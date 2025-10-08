//José Manuel Díaz Hernández y Nicolás Rey Alonso, Práctica 1A, IC 2025

#include <time.h>
#include <RTCZero.h>
#include <Arduino_MKRMEM.h>
#include <ArduinoLowPower.h>

#define N 10  //Numero de iteraciones antes de mostrar por pantalla el contenido de la memoria

RTCZero rtc;
volatile uint32_t _period_sec = 10;
volatile uint8_t _rtcFlag = 0;
volatile uint8_t _pinFlag = 0;
volatile uint8_t i = 0;
volatile uint32_t nextAlarm;

Arduino_W25Q16DV flash(SPI1, FLASH_CS);
char filename[] = "Hora.txt";
const int externalPin = 5;

void setup() {
  SerialUSB.begin(9600);
  pinMode(externalPin, INPUT_PULLUP); 
  LowPower.attachInterruptWakeup(externalPin, externalCallback, FALLING); 
  pinMode(LORA_RESET, OUTPUT);    // Declaramos LORA reset pin como salida
  digitalWrite(LORA_RESET, LOW);  // Lo ponemos a nivel bajo para desactivar el módulo 
  while(!SerialUSB) {;}
  flash.begin();
  rtc.begin();
  if (!setDateTime(__DATE__, __TIME__)) {
    SerialUSB.println("setDateTime() failed!");
    while (1);
  }
  SerialUSB.println("Mounting filesystem");
  int res = filesystem.mount();
  if(res != SPIFFS_OK && res != SPIFFS_ERR_NOT_A_FS) {
    SerialUSB.println("mount() failed with error code "); 
    SerialUSB.println(res); 
    while(1) {
      SerialUSB.println("Fatal error, halted.");
      delay(1000);
    }
  }
    SerialUSB.println("Opening or creating file");
  File file = filesystem.open(filename,  CREATE | READ_WRITE);
  if (!file) {
    SerialUSB.print("Creation of file ");
    SerialUSB.print(filename);
    SerialUSB.print(" failed. Aborting ...");
    on_exit_with_error_do();
  }
  file.close();
  SerialUSB.println("Reading filecontents");
  readFileContents();
  LowPower.attachInterruptWakeup(RTC_ALARM_WAKEUP, alarmCallback, CHANGE);
  setPeriodicAlarm(_period_sec, 5);
  nextAlarm = rtc.getEpoch() + _period_sec;
  rtc.setAlarmEpoch(nextAlarm);
  rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);

}

void loop() {
  if (_rtcFlag) {
    _rtcFlag = 0;
    while (!SerialUSB);
    printDateTime(false, rtc.getEpoch());
    // Mensajes de depuración
    while (!SerialUSB);
    SerialUSB.print("Iter ");
    SerialUSB.println(i, DEC);
    if (i >= 10) {
      i = 0;
      readFileContents();
    }
    i++;
    // Siguiente alarma fija cada 10 s exactos
    nextAlarm += _period_sec;
    rtc.setAlarmEpoch(nextAlarm);   
  }

  if (_pinFlag) {
    _pinFlag = 0;
    while (!SerialUSB);
    printDateTime(true, rtc.getEpoch());   
  }

  SerialUSB.end();
  LowPower.sleep();
  SerialUSB.begin(9600);
}
void readFileContents(){
  File file = filesystem.open(filename, READ_WRITE);
  long size = file.lseek(0, END); // Compruebo el tamaño del fichero ya que file.size no existe
  if (file) {
    if (size == 0) {
      Serial.println("File is empty");
    } else {
      file.lseek(0, START); 
      while(!file.eof()) {
        char c;
        int const bytes_read = file.read(&c, sizeof(c));
        if (bytes_read) {
          SerialUSB.print(c);
          if (c == '\n') {SerialUSB.print("\t ");}
        }
      }
    }
  }
}


bool setDateTime(const char * date_str, const char * time_str) {
  char month_str[4];
  char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  uint16_t i, mday, month, hour, min, sec, year;

  if (sscanf(date_str, "%3s %hu %hu", month_str, &mday, &year) != 3) return false;
  if (sscanf(time_str, "%hu:%hu:%hu", &hour, &min, &sec) != 3) return false;

  for (i = 0; i < 12; i++) {
    if (!strncmp(month_str, months[i], 3)) {
      month = i + 1;
      break;
    }
  }
  if (i == 12) return false;

  rtc.setTime((uint8_t)hour, (uint8_t)min, (uint8_t)sec);
  rtc.setDate((uint8_t)mday, (uint8_t)month, (uint8_t)(year - 2000));
  return true;
}

void setPeriodicAlarm(uint32_t period_sec, uint32_t offsetFromNow_sec) {
  rtc.setAlarmEpoch(rtc.getEpoch() + offsetFromNow_sec);
  rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);
}

void alarmCallback() {
  _rtcFlag = 1;
}

void externalCallback() {
  _pinFlag = 1;
}

void printDateTime(bool fromExternal, time_t epoch) {
  const char *weekDay[7] = { "Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat" };
  struct tm stm;
  gmtime_r(&epoch, &stm);
  char dateTime[32];

  if (fromExternal) {
    snprintf(dateTime, sizeof(dateTime), "[EXT] %s %4u/%02u/%02u %02u:%02u:%02u\n",
             weekDay[stm.tm_wday],
             stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday,
             stm.tm_hour, stm.tm_min, stm.tm_sec);
  } else {
  snprintf(dateTime, sizeof(dateTime), "%s %4u/%02u/%02u %02u:%02u:%02u\n",
           weekDay[stm.tm_wday],
           stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday,
           stm.tm_hour, stm.tm_min, stm.tm_sec);
  }
  writeInMem(dateTime);
}
bool writeInMem(char* data_line){
    File file = filesystem.open(filename, READ_WRITE);
    if (!file) {
      SerialUSB.print("Open file ");
      SerialUSB.print(filename);
      SerialUSB.print(" failed. Aborting ...");
      on_exit_with_error_do();
    }
    file.lseek(0, END);
    int const bytes_to_write = strlen(data_line);
    int const bytes_written = file.write((void *)data_line, bytes_to_write);
    if (bytes_to_write != bytes_written) {
      SerialUSB.print("write() failed with error code "); 
      SerialUSB.println(filesystem.err());
      SerialUSB.println("Aborting ...");
      on_exit_with_error_do();
    }
    file.close();
  return true;
}
void on_exit_with_error_do()
{
  filesystem.unmount();
  while(1) {
    SerialUSB.println("Fatal error, halted.");
    delay(1000);
  }
}