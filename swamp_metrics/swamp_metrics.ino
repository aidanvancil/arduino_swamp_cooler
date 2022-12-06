/*--------------------------------------------------
  Author:   Aidan Vancil + Andrew Gorum
  Project:  Creating A Embedded System (Swamp Cooler)
  Commentars: N/A
  Date:     11/09/22
  ---------------------------------------------------*/
// Internal Libraries
#include <LiquidCrystal.h>
#include <DHT.h>
#include <Wire.h>
#include "RTClib.h"
#include <DHT_U.h>
#include <Stepper.h>

#define RDA 0x80
#define TBE 0x20

// Temperature / Humidity
float temp;
#define DHTPIN 6       // DHT Pin
#define DHTTYPE DHT11  // DHT Type

// Initialize the DHT11 sensor on DHTPIN 6
DHT dht(DHTPIN, DHTTYPE);

// Initialize the stepper library on pins 2 through 5:
const float STEPS_PER_REV = 32;
Stepper myStepper(STEPS_PER_REV, 2, 4, 3, 5);

// Initialize the LCD
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Initialize a non-blocking delay
unsigned long int delayStart = millis();

// Define Port A Register Pointers
volatile unsigned char *port_a = (unsigned char *)0x22;
volatile unsigned char *ddr_a = (unsigned char *)0x21;
volatile unsigned char *pin_a = (unsigned char *)0x20;

// Define Port B Register Pointers
volatile unsigned char *port_b = (unsigned char *)0x25;
volatile unsigned char *ddr_b = (unsigned char *)0x24;
volatile unsigned char *pin_b = (unsigned char *)0x23;

// Define Port D Register Pointers
volatile unsigned char *port_d = (unsigned char *)0x2B;
volatile unsigned char *ddr_d = (unsigned char *)0x2A;
volatile unsigned char *pin_d = (unsigned char *)0x29;

// Define Port K Register Pointers
volatile unsigned char *port_k = (unsigned char *)0x108;
volatile unsigned char *ddr_k = (unsigned char *)0x107;
volatile unsigned char *pin_k = (unsigned char *)0x106;

// Define UART Pointers
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int *myUBRR0 = (unsigned int *)0x00C4;
volatile unsigned char *myUDR0 = (unsigned char *)0x00C6;

// Define ADC Pointers
volatile unsigned char *my_ADMUX = (unsigned char *)0x7C;
volatile unsigned char *my_ADCSRB = (unsigned char *)0x7B;
volatile unsigned char *my_ADCSRA = (unsigned char *)0x7A;
volatile unsigned int *my_ADC_DATA = (unsigned int *)0x78;

// Macros
#define WRITE_HIGH_PD(pin_num) *port_d |= (0x01 << pin_num);
#define WRITE_LOW_PD(pin_num) *port_d &= ~(0x01 << pin_num);
#define WRITE_HIGH_PA(pin_num) *port_a |= (0x01 << pin_num);
#define WRITE_LOW_PA(pin_num) *port_a &= ~(0x01 << pin_num);
#define WRITE_HIGH_PK(pin_num) *port_k |= (0x01 << pin_num);
#define WRITE_LOW_PK(pin_num) *port_k &= ~(0x01 << pin_num);

// Constants
bool errMessage = false;
unsigned int currentTicks;
bool timer_running;
bool time_switch = false;
bool startButton = false;
bool idleState = false;
bool fan_running = false;
bool enabled = false;
unsigned long int TEMP_THRESHOLD = 72;
unsigned long int WATER_THRESHOLD = 0; // was 250

RTC_DS1307 rtc;//define a object of RTC_DS1307 class
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup() {
   pinMode(23, OUTPUT);
   pinMode(25, OUTPUT);
   pinMode(27, OUTPUT);
//  (Pin 4) [Yellow] - Disabled / Fan Off Running
//  (Pin 5) [Blue] - Fan Running
//  (Pin 6) [Red] - Error
//  (Pin 7) [Green] - Fan Off Running

  U0init(9600); // setup the UART
   
  // External Modules : DHT11
  dht.begin();

  // External Modules : LCD
  lcd.begin(16, 2);
  lcd.clear();

  // External Modules : Stepper
  myStepper.setSpeed(200);

  // External Modules : ADC
  adc_init();

  // External Modules : RTC
  //rtc.adjust(DateTime(2022, 12, 6, 10, 4, 20));  
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  bool start_button = *pin_d & 0b00000100;
  if(start_button & !enabled){
    enabled = true;
  } else if (start_button){
    errMessage = false;
    enabled = false;
  }
  // 23: OFF / ON
  // 25: DIRECTION 1
  // 27: DIRECTION 2
  
  // if (enabled && !errMessage) {
  //   Serial.println("Fan running");
  //   digitalWrite(23, HIGH);
  //   digitalWrite(25, HIGH);
  //   digitalWrite(27, LOW);
  // } else if (enabled) {
  //   digitalWrite(23, LOW);
  //   Serial.println("Fan not running");
  // }
 
  // External Modules : Stepper Direction
  bool button_1 = *pin_d & 0b00000010;
  bool button_2 = *pin_d & 0b00000001;
  if (!errMessage){
    if (button_1) {
      myStepper.step(50);
    } else if (button_2) {
      myStepper.step(-50);
    }
  }

  // External Modules : DHT / LCD Clearing
  if (!errMessage && ((millis() - delayStart) >= 60000) && !enabled) {
    lcd.clear();
    delayStart = millis();
  } else if (!errMessage && ((millis() - delayStart) >= 60000) && enabled) {
    lcd.clear();
    delayStart = millis();
    float humidity = dht.readHumidity();
    float temp_celsius = dht.readTemperature();
    float temp_farenheit = dht.readTemperature(true);
    lcd.print("Temp   : ");
    lcd.print(temp_farenheit);
    if (temp_farenheit > TEMP_THRESHOLD) {
      idleState = false;
    } else {
      idleState = true;
    }
    lcd.print("\337F");
    lcd.setCursor(0, 1);
    lcd.print("Humid %: ");
    lcd.print(humidity);
  }

  // External Modules : ADC
  unsigned int adc_reading = adc_read(0);
  if (enabled && !idleState && adc_reading < WATER_THRESHOLD){
    errMessage = true;
  }
  if ((adc_reading <= WATER_THRESHOLD) && enabled){
    idleState = false;
    errMessage = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Water Level");
    lcd.setCursor(0, 1);
    lcd.print("Too Low...");
  }

  if (enabled && !errMessage) {
    if (!time_switch){
      Serial.println("Fan On And Running @");
      printTime();
      time_switch = true;
    }
    digitalWrite(23, HIGH);
    digitalWrite(25, HIGH);
    digitalWrite(27, LOW);
  } else if (enabled) {
    digitalWrite(23, LOW);
    if (time_switch){
      Serial.print("Fan On & Idle / Err @");
      printTime();
      time_switch = false;
    }
  } else {
    time_switch = false;
    Serial.println("System Disabled");
  }
  if (!enabled){
    lcd.clear();
  }

  lightShow();
  delay(2000);
}

void lightShow() {
 
  if (errMessage) {
    WRITE_HIGH_PK(6);
  } else {
    WRITE_LOW_PK(6);
  }
 
  if (idleState & !errMessage) {
    WRITE_HIGH_PK(7);
  } else {
    WRITE_LOW_PK(7);
  }

  if (!enabled) {
    WRITE_HIGH_PK(4);
  } else {
    WRITE_LOW_PK(4);
  }

  if (!idleState && enabled && !errMessage){
    WRITE_HIGH_PK(5);
  } else {
    WRITE_LOW_PK(5);
  }
}

// External Functions
void printTime(){
  DateTime now = rtc.now();
  Serial.println(now.year(), DEC);
  Serial.println('/');
  Serial.println(now.month(), DEC);
  Serial.println('/');
  Serial.println(now.day(), DEC);
  Serial.println(" (");
  Serial.println(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.println(") ");
  Serial.println(now.hour(), DEC);
  Serial.println(':');
  Serial.println(now.minute(), DEC);
  Serial.println(':');
  Serial.println(now.second(), DEC);
  Serial.println();
}

void adc_init() {
  *my_ADCSRA |= 0b10000000;
  *my_ADCSRA &= 0b11011111;
  *my_ADCSRA &= 0b11110111;
  *my_ADCSRA &= 0b11111000;
  *my_ADCSRB &= 0b11110111;
  *my_ADCSRB &= 0b11111000;
  *my_ADMUX &= 0b01111111;
  *my_ADMUX |= 0b01000000;
  *my_ADMUX &= 0b11011111;
  *my_ADMUX &= 0b11100000;
}
unsigned int adc_read(unsigned char adc_channel_num) {
  *my_ADMUX &= 0b11100000;
  *my_ADCSRB &= 0b11110111;
  if (adc_channel_num > 7) {
    adc_channel_num -= 8;
    *my_ADCSRB |= 0b00001000;
  }
  *my_ADMUX += adc_channel_num;
  *my_ADCSRA |= 0x40;
  while ((*my_ADCSRA & 0x40) != 0);
  return *my_ADC_DATA;
}

void print_int(unsigned int out_num) {
  unsigned char print_flag = 0;
  if (out_num >= 1000) {
    U0putchar(out_num / 1000 + '0');
    print_flag = 1;
    out_num = out_num % 1000;
  }
  if (out_num >= 100 || print_flag) {
    U0putchar(out_num / 100 + '0');
    print_flag = 1;
    out_num = out_num % 100;
  }
  if (out_num >= 10 || print_flag) {
    U0putchar(out_num / 10 + '0');
    print_flag = 1;
    out_num = out_num % 10;
  }
  U0putchar(out_num + '0');
  U0putchar('\n');
}

void U0init(int U0baud) {
  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1);
  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0 = tbaud;
  Serial.begin(9600);
}
unsigned char U0kbhit() {
  return *myUCSR0A & RDA;
}
unsigned char U0getchar() {
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata) {
  while ((*myUCSR0A & TBE) == 0);
  *myUDR0 = U0pdata;
}
