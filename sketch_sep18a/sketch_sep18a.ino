#include <time.h>
#include <RTCZero.h>
#include <Arduino_MKRMEM.h>

RTCZero rtc;
volatile uint32_t _period_sec = 10;
volatile uint8_t _rtcFlag = 0;
Arduino_W25Q16DV flash(SPI1, FLASH_CS);
char filename[] = "Hora.txt";

void setup() {
  SerialUSB.begin(9600);
  pinMode(LORA_RESET, OUTPUT);    // Declaramos LORA reset pin como salida
  digitalWrite(LORA_RESET, LOW);  // Lo ponemos a nivel bajo para desactivar el módulo 
  while(!SerialUSB) {;}
    flash.begin();
  rtc.begin();
  if (!setDateTime(__DATE__, __TIME__)) {
    SerialUSB.println("setDateTime() failed!");
    while (1);
  }
  SerialUSB.println("Mountaje del sistema de ficheros");
  int res = filesystem.mount();
  if(res != SPIFFS_OK && res != SPIFFS_ERR_NOT_A_FS) {
    SerialUSB.println("mount() failed with error code "); 
    SerialUSB.println(res); 
    exit(EXIT_FAILURE);
  }
  File file = filesystem.open(filename,  CREATE | TRUNCATE);
  if (!file) {
    SerialUSB.print("Creation of file ");
    SerialUSB.print(filename);
    SerialUSB.print(" failed. Aborting ...");
    on_exit_with_error_do();
  }
  file.close();

  setPeriodicAlarm(_period_sec, 5);
  rtc.attachInterrupt(alarmCallback);

}

void loop() {
  SerialUSB.begin(9600);
  SerialUSB.println("Entrando en standby...");
  SerialUSB.end();
  rtc.standbyMode();   // duerme hasta la alarma

  if (_rtcFlag) {
    _rtcFlag = 0;
    printDateTime();   // imprime fuera de la ISR
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
  rtc.setAlarmEpoch(rtc.getEpoch() + _period_sec);
}

void printDateTime() {
  const char *weekDay[7] = { "Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat" };
  time_t epoch = rtc.getEpoch();
  struct tm stm;
  gmtime_r(&epoch, &stm);
  char dateTime[32];
  SerialUSB.begin(9600);
  snprintf(dateTime, sizeof(dateTime), "%s %4u/%02u/%02u %02u:%02u:%02u",
           weekDay[stm.tm_wday],
           stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday,
           stm.tm_hour, stm.tm_min, stm.tm_sec);
  SerialUSB.println(dateTime);
  SerialUSB.end();
}
