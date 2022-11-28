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
//#include "DS1307.h"
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

// Define UART Pointers
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int *myUBRR0 = (unsigned int *)0x00C4;
volatile unsigned char *myUDR0 = (unsigned char *)0x00C6;

// Define Timer Pointers
volatile unsigned char *myTCCR1A  = 0x80;
volatile unsigned char *myTCCR1B  = 0x81;
volatile unsigned char *myTCCR1C  = 0x82;
volatile unsigned char *myTIMSK1  = 0x6f;
volatile unsigned char *myTIFR1   = 0x36;
volatile unsigned int  *myTCNT1   = 0x84;

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

// Constants
bool errMessage = false;
bool canRun = false;
unsigned int currentTicks;
bool timer_running;
//DS1307 clock;//define a object of DS1307 class

void setup() {
//  *ddr_a |= 0x01 << 0b00000001;  // Output Port A23 (Pin 1) [Yellow] - Disabled / Fan Off Running
//  *ddr_a |= 0x01 << 0b00000011;  // Output Port A25 (Pin 3) [Blue] - Fan Running
//  *ddr_a |= 0x01 << 0b00000101;  // Output Port A27 (Pin 5) [Red] - Error
//  *ddr_a |= 0x01 << 0b00000111;  // Output Port A29 (Pin 7) [Green] - Fan Off Running
  
  setup_timer_regs(); // setup Timer for Normal Mode, TOV Enabled
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
//  clock.begin();
//  clock.fillByYMD(2022,11,23);//Nov 23,2022
//  clock.fillByHMS(9,01,00);//09:01 00
//  clock.fillDayOfWeek(WED);//
//  clock.setTime();//write time to the RTC chip
  
}

void loop() {
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
  if (!errMessage && ((millis() - delayStart) >= 5000)) {
    lcd.clear();
    delayStart = millis();
    float humidity = dht.readHumidity();
    float temp_celsius = dht.readTemperature();
    float temp_farenheit = dht.readTemperature(true);
    lcd.print("Temp   : ");
    lcd.print(temp_farenheit);
    lcd.print("\337F");
    lcd.setCursor(0, 1);
    lcd.print("Humid %: ");
    lcd.print(humidity);
  }

  // External Modules : Fan Motor / Running / Stopping
  //if (canRun & startButton || canRun & idleState){
    
  //}

  // External Modules : ADC
  unsigned int adc_reading = adc_read(0);
  if (adc_reading < 250) {
    canRun = false;
    errMessage = true;
    *ddr_a |= 0x01 << 0b00000101;
    WRITE_HIGH_PA(5);
    //printTime();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Water Level");
    lcd.setCursor(0, 1);
    lcd.print("Too Low...");
  } else {
    canRun = true;
    errMessage = false;
    *ddr_a |= 0x01 << 0b00000101;
    WRITE_LOW_PA(5);
  }
  
  // External Modules : Interrupts (Start Button)
  if(start_button && started) {
    currentTicks = 65535; // Disable system
    if(timer_running) {
      *myTCCR1B &= 0xF8;
      timer_running = 0;
      *portB &= 0xBF;
    }
  } else if (start_button && !started) {
    currentTicks = ((1.0/double(start_button)/2.0f))/0.0000000625;
    if(!timer_running) {
      *myTCCR1B |= 0x01;
      timer_running = 1;
    }
  }
}

void setup_timer_regs() {
  // setup the timer control registers
  *myTCCR1A= 0x00;
  *myTCCR1B= 0X00;
  *myTCCR1C= 0x00;

  *myTIFR1 |= 0x01; // reset the TOV flag
  *myTIMSK1 |= 0x01; // enable the TOV interrupt
}

ISR(TIMER1_OVF_vect){
  *myTCCR1B &=0xF8;
  *myTCNT1 =  (unsigned int) (65535 -  (unsigned long) (currentTicks));
  *myTCCR1B |=   0x01;
  if(currentTicks != 65535) {
    *port_b^= 0x40;
  }
}



// External Functions

//void printTime(){
//    clock.getTime();
//    Serial.print(clock.hour, DEC);
//    Serial.print(":");
//    Serial.print(clock.minute, DEC);
//    Serial.print(":");
//    Serial.print(clock.second, DEC);
//    Serial.print("  ");
//    Serial.print(clock.month, DEC);
//    Serial.print("/");
//    Serial.print(clock.dayOfMonth, DEC);
//    Serial.print("/");
//    Serial.print(clock.year+2000, DEC);
//    Serial.print(" ");
//    Serial.print(clock.dayOfMonth);
//    Serial.println(" ");
//}

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
