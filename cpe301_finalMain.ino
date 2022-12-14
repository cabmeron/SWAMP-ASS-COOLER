// CPE 301: Embedded Syystems 
// Fall 2022
// by: Dillon Dutcher, Cameron McCoy, Luis Ramirez Torres
// Instructor: Shawn Ray
// University of Nevada, Reno


#include <Wire.h>
#include <Adafruit_Sensor.h> //needed for DHT library
#include <DHT.h>
#include <DHT_U.h>
#include <LiquidCrystal.h>
#include <Stepper.h>
#include <RTClib.h>



#define RDA 0x80
#define TBE 0x20
#define STEPS 32

volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;
 
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

volatile unsigned char *portDDRB = (unsigned char *) 0x24;
volatile unsigned char *portB =    (unsigned char *) 0x25;
volatile unsigned char *portH = (unsigned char *) 0x102;
volatile unsigned char *portDDRH = (unsigned char *) 0x101;

int state = 0;

//setting up the lcd display
const int rs = 12, en = 33, d4 = 6, d5 = 5, d6 = 4, d7 = 3;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//setting up the DHT sensor
#define DHTPIN 7
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
float humidity;
float temperature;


//setting up the stepper motor
Stepper stepper(STEPS, 23, 27, 25, 29);
int pVal = 0;
int potVal = 0;


void setup() {
  // put your setup code here, to run once:
  U0init(9600);

  lcd.begin(16, 2);
  dht.begin();
  adc_init();
  stepper.setSpeed(200);
  *portDDRB |= 0b11110000;
  *portDDRH |= 0b01100000;
  attachInterrupt(digitalPinToInterrupt(2), isr, RISING);
}

void loop() {
  // put your main code here, to run repeatedly:


  if(state == 0){
    disabled();
  }
  else{
    *portH |= 0b01000000;
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    unsigned int waterLevel = adc_read(5);
    unsigned int potReading = adc_read(0);
    potVal = map(potReading, 0, 1024, 0, 500);
    
    if(potVal > pVal){
      stepper.step(20);
      //printTime();
    }
    if(potVal < pVal){
      stepper.step(-20);
      //printTime();
    }
    pVal = potVal;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("temp: ");
    lcd.print(temperature);
    lcd.setCursor(0, 1);
    lcd.print("humidity: ");
    lcd.print(humidity);
    powerMotor(temperature);
    //printWarning(waterLevel);
  }
}


void adc_init()
{
  // setup the A register

  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &=  0b11011111;// clear bit 5 to 0 to disable the ADC trigger mode
  *my_ADCSRA &=  0b11011111;// clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &=  0b11011111;// clear bit 5 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &=  0b11110000;// clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &=  0b11111000;// clear bit 2-0 to 0 to set free running mode

  // setup the MUX Register
  
  *my_ADMUX  &= 0b01111111;// clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &=  0b11011111;// clear bit 5 to 0 for right adjust result
  *my_ADMUX  &=  0b11011111;// clear bit 5 to 0 for right adjust result
  *my_ADMUX  &=  0b11100000;// clear bit 4-0 to 0 to reset the channel and gain bits
}


// ADC Read

unsigned int adc_read(unsigned char adc_channel_num)
{
  
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;

  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11011111;
  
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;

    // set MUX bit 5
    *my_ADCSRB |= 0b00100000;
  }

  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;

  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);

  // return the result in the ADC data register
  return *my_ADC_DATA;

}


void print_int(unsigned int out_num)
{
  // clear a flag (for printing 0's in the middle of numbers)
  unsigned char print_flag = 0;

  // if its greater than 1000
  if(out_num >= 1000)
  {
    // get the 1000's digit, add to '0' 
    U0putchar(out_num / 1000 + '0');

    // set the print flag
    print_flag = 1;

    // mod the out num by 1000
    out_num = out_num % 1000;

  }

  // if its greater than 100 or we've already printed the 1000's
  if(out_num >= 100 || print_flag)
  {
    // get the 100's digit, add to '0'
    U0putchar(out_num / 100 + '0');
    // set the print flag
    print_flag = 1;
    // mod the output num by 100
    out_num = out_num % 100;
  } 
  // if its greater than 10, or we've already printed the 10's
  if(out_num >= 10 || print_flag)
  {
    U0putchar(out_num / 10 + '0');
    print_flag = 1;
    out_num = out_num % 10;
  } 
  // always print the last digit (in case it's 0)
  U0putchar(out_num + '0');
  // print a newline
  U0putchar('\n');
}

void U0init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}

unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}

unsigned char U0getchar()
{
  return *myUDR0;
}

void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}


void motor_intialization(){
  *portDDRB |= 0b10000000;
}

void powerMotor(float temp){
  if(temp > 20){
    //printTime();
    *portB |= 0b10000000;
    *portH |= 0b00100000;
    *portB &= 0b10001111;
    *portH &= 0b10100000;
  }
  else{
    *portB &= 0b01111111;
  }
  
}


void printWarning(unsigned int waterLevel){
  if(waterLevel < 100){
    //printTime();
    char warning[11] = "water low \n";
    for(int i = 0; i < 11; i++){
      U0putchar(warning[i]);
      U0getchar();
    }
    while(adc_read(5) < 100 && state != 0){
      *portB |= 0b00100000;
      *portB &= 0b00101111;
      *portH &= 0b10011111;
    }
    //printTime();
    *portB &= 0b11011111;
  }
  delay(1000);
}

void disabled(){
  *portB |= 0b00010000;
  *portB &= 0b00011111;
  *portH &= 0b10011111;
  lcd.clear();
}

void isr(){
  if(state != 0){
    disabled();
    state = 0;
  }
  else{
    state = 1;
    *portB &= 0b11101111;
    *portH |= 0b01000000;
  }
}
