/*
  papracode
    - Reads battery voltage and lights up LEDs to mimic M12 battery fuel gauge
    - Reads user pot to control PWM of fan

*/

// Compile with Arduino 1.8.13 or later.
//
// Xiao ESP32C3 3.3V

#include "driver/ledc.h"

#define pwmChannel 0
#define PWM_Res 8
#define PWM_Freq 1200

// Pin connections per PAPR TH PCB
// A0 - ADC - POTENTIOMETER
// A1 - ADC - BATTERY
// D2 - LED3
// D3 - LED1
// D4 - N/C
// D5 - N/C
// D6 - N/C
// D7 - LED2
// D8 - LED4
// D9 - FAN-PWM
// D10 - BUZZER

int analogPot = A0;   // A0 - Fan speed control POT with on/off switch
int analogBatt = A1;  // A1 - 10 bit resolution on ADC
int led1 = D3;
int led2 = D7;
int led3 = D2;
int led4 = D8;
int PWMPin = D9;
int buzzerPin = D10;

// Battery Voltage to Fuel Gauge
// 12.36V = 4.12V/Cell = 100% - 78% Battery  = 4 LEDs         => 3951 > adc > 3743
// 11.85V = 3.95V/Cell =  77% - 55% Battery  = 3 LEDs         => 3742 > adc > 3468
// 11.10V = 3.70V/Cell =  54% - 33% Battery  = 2 LEDs         => 3467 > adc > 3307
// 10.62V = 3.54V/Cell =  32% - 10% Battery  = 1 LEDs         => 3306 > adc > 3017
//  9.75V = 3.25V/Cell =  10% - 0%  Battery  = LEDS FLASHING  => 3016 > adc > 2588
//  8.40V = 2.80V/Cell =  0%        Battery  = Shut Down

const int battADCMax = 4095;
const int battADCFull = 3951;
const int battADC78p = 3742;
const int battADC55p = 3467;
const int battADC33p = 3306;
const int battADC10p = 3016;
const int battADC0p = 2588;
const int battADCMin = 0;

// MAP FOR CUI 154 BLOWER
// Limits the min pot value     Voltage   Min PWM Percentage
// const int minPWMfull = 108;  // 11.85V      69     >77%
// const int minPWM75p  = 112;  // 11.10V      72     >54%
// const int minPWM50p  = 115;  // 10.62V      77     >33%
// const int minPWM25p  = 120;  // 9.75V       88     >10%
// const int minPWM10p  = 250;  // 8.40V      110     >0%
// These values were determined emperically by adjusting the input voltage and dialing the PWM down until motor stall

// MAP FOR NM33GA-12Q-AT BLOWER
// Limits the min pot value     Voltage   Min PWM Percentage
//const int minPWMfull = 10;  // 11.85V      69     >77%
//const int minPWM75p = 11;   // 11.10V      72     >54%
//const int minPWM50p = 15;   // 10.62V      77     >33%
//const int minPWM25p = 20;   // 9.75V       88     >10%
//const int minPWM10p = 25;   // 8.40V      110     >0%
// These values were determined emperically by adjusting the input voltage and dialing the PWM down until motor stall

// MAP FOR SEEED ESP32-C3 SANYO BLOWER 9BMB12H202 (two wire)
// Limits the min pot value     Voltage   Min PWM Percentage
//const int minPWMfull = 219;  // 11.85V      69     >77%
//const int minPWM75p = 222;   // 11.10V      72     >54%
//const int minPWM50p = 223;   // 10.62V      77     >33%
//const int minPWM25p = 225;   // 9.75V       88     >10%
//const int minPWM10p = 232;   // 8.40V      110     >0%

// MAP FOR SEEED ESP32-C3 SANYO BLOWER 9BMB12F201 (three wire, only two used, yellow unattached)
// Limits the min pot value     Voltage   Min PWM Percentage
const int minPWMfull = 220;  // 11.85V      69     >77%
const int minPWM75p = 225;   // 11.10V      72     >54%
const int minPWM50p = 227;   // 10.62V      77     >33%
const int minPWM25p = 228;   // 9.75V       88     >10%
const int minPWM10p = 230;   // 8.40V      110     >0%

// to fill out these charts:
// change your min pwm to 10 for all values
// connect the board to a power supply
// connect the usb-c to the arduino firmware system via a usb cable with no power supplied
// flash the firmware with the pwm set to 10
// set your power supply to the right voltage (11.85, 11.10, etc)
// move the potentiometer until the fan stalls
// change voltage, repeat, capturing the PWM value from the Serial Monitor output
// if your values are above 200, like they are for the sanyo blowers, you may want to 
// run the process multiple times to get better dynamic range.


// State Machine states
const int batteryCheck = 7;
const int batteryFull = 6;
const int battery75p = 5;
const int battery50p = 4;
const int battery25p = 3;
const int battery10p = 2;
const int batteryDead = 1;

int batteryState = batteryCheck;

int offTime = 5;     // ms
int onTime = 95;     // ms
int loopDelay = 25;  // ms
int blinkCounter = 0;
const uint32_t numBatterySamples = 10;
uint32_t battery = 4095;
const uint32_t maxPot = 1023;
const uint32_t minPot = 0;
uint32_t fanPWM = 0;
uint32_t minPWM = minPWM10p;
uint32_t maxPWM = 256; // pow(2, PWM_Res);  // replaced for readability
uint32_t rawADC = 0;
uint32_t vILM = 0;

const int LEDFlashLoop = 25;  // decrease for 10% battery LED to blink faster

// the setup routine runs once when you press reset:
void setup() {
  // initialize the digital pin as an output.
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(led4, OUTPUT);
  pinMode(analogPot, INPUT);
  pinMode(analogBatt, INPUT);
  pinMode(buzzerPin, OUTPUT);
  ledcAttachPin(PWMPin, pwmChannel);
  ledcSetup(pwmChannel, PWM_Freq, PWM_Res);

  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
  digitalWrite(led3, LOW);
  digitalWrite(led4, LOW);
  digitalWrite(buzzerPin, HIGH);
  ledcWrite(pwmChannel, maxPWM);  // Turn on fan 100%
  Serial.begin(115200);
  Serial.println("Starting up");
  Serial.println("PAPRA TH 18FEB2024");
  Serial.println("PCB v1");
  Serial.println("ESP32C3 PROCESSOR\nSANYO BLOWER 1688-1376-ND");
  Serial.println("Tetra Bio Distributed");
  for (int i = 0; i <= 4; i++) {  // startup knightrider
    digitalWrite(led1, HIGH);
    delay(onTime);
    digitalWrite(buzzerPin, LOW);
    digitalWrite(led1, LOW);
    digitalWrite(led2, HIGH);
    delay(onTime);
    digitalWrite(led2, LOW);
    digitalWrite(led3, HIGH);
    delay(onTime);
    digitalWrite(led3, LOW);
    digitalWrite(led4, HIGH);
    delay(onTime);
    delay(onTime);
    digitalWrite(led4, LOW);
    digitalWrite(led3, HIGH);
    delay(onTime);
    digitalWrite(led3, LOW);
    digitalWrite(led2, HIGH);
    delay(onTime);
    digitalWrite(led2, LOW);
    digitalWrite(led1, HIGH);
    delay(onTime);
    digitalWrite(led1, LOW);
    delay(offTime);
  }
}

// the loop routine runs over and over again forever:
void loop() {
  delay(125);

  // Clamp and rescale potentiometer input from a 10bit value to 8 bit
  if (maxPWM > 0) {
    rawADC = analogRead(analogPot) >> 2;
    fanPWM = map(rawADC, minPot, maxPot, minPWM, maxPWM);  // scale knob, 10 bits to 8 bits (v0.3 change)
  } else {
    fanPWM = maxPWM;
  }
  ledcWrite(pwmChannel, fanPWM);

  uint32_t rawBatteryValue = analogRead(analogBatt);
  battery = ( ( battery * ( numBatterySamples - 1 ) ) + ( rawBatteryValue ) ) / numBatterySamples;

  Serial.printf("Pot:%d,PWM:%d,BatRaw:%d,Batt:%d\n", rawADC, fanPWM, rawBatteryValue, battery );

  switch( battery ) {
    case battADC78p ... battADCMax:  // Full = 78% - 100%
      if (batteryState > batteryFull) {
        batteryState = batteryFull;
        digitalWrite(led1, HIGH);  // All LEDs ON
        digitalWrite(led2, HIGH);
        digitalWrite(led3, HIGH);
        digitalWrite(led4, HIGH);
        minPWM = minPWMfull;
      }
      break;
    case battADC55p ...(battADC78p - 1):  // 75% = 55% - 77%
      if (batteryState > battery75p) {
        batteryState = battery75p;
        digitalWrite(led1, HIGH);  // 3 LEDs ON
        digitalWrite(led2, HIGH);
        digitalWrite(led3, HIGH);
        digitalWrite(led4, LOW);
        minPWM = minPWM75p;
      }
      break;
    case battADC33p ...(battADC55p - 1):  // 50% = 33% - 54%
      if (batteryState > battery50p) {
        batteryState = battery50p;
        digitalWrite(led1, HIGH);  // 2 LEDs ON
        digitalWrite(led2, HIGH);
        digitalWrite(led3, LOW);
        digitalWrite(led4, LOW);
        minPWM = minPWM50p;
      }
      break;
    case battADC10p ...(battADC33p - 1):  // 25% = 10% - 32%
      if (batteryState > battery25p) {
        batteryState = battery25p;
        digitalWrite(led1, HIGH);  // 1 LED on
        digitalWrite(led2, LOW);
        digitalWrite(led3, LOW);
        digitalWrite(led4, LOW);
        minPWM = minPWM25p;
      }
      break;
    case battADC0p ...(battADC10p - 1):  // 10% - Need to blink LED
      if (batteryState > battery10p) {
        batteryState = battery10p;
        digitalWrite(led1, HIGH);  // 1 LED blinking
        digitalWrite(led2, LOW);
        digitalWrite(led3, LOW);
        digitalWrite(led4, LOW);
        digitalWrite( buzzerPin, HIGH );
        minPWM = minPWM10p;
      }
      break;
    case battADCMin ...(battADC0p - 1):  // Shutdown
      if (batteryState > batteryDead) {
        batteryState = batteryDead;
        digitalWrite(led1, LOW);  // All LEDs off
        digitalWrite(led2, LOW);
        digitalWrite(led3, LOW);
        digitalWrite(led4, LOW);
        maxPWM = 255;  // turn off fan - overrides the measuremnt from above
      }
      break;
  }
  // For battery state of 0-10%, need to blink LED1 to indicate almost dead battery
  if( batteryState == battery10p ) {
    if (blinkCounter++ > LEDFlashLoop) {
      digitalWrite(led1, !digitalRead(led1));
      blinkCounter = 0;
    }
  }
}