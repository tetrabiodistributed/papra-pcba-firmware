/*
  papracode
    - Reads batteryADCSampleAverage voltage and lights up LEDs to mimic M12 batteryADCSampleAverage fuel gauge
    - Reads user pot to control PWM of fan

  PCB is compatible with all 0/1/2-Series 14 pin ATtiny chips
  For chips with 2K flash (ATtiny204 & ATtiny404), the Preprocessor directive must be uncommented
*/

#include <Arduino.h>

//#define TWOKFLASHCHIP // uncomment line when programming devices with 2K of flash - ATtiny204 or ATtiny214
#define FOURKORBIGGERFLASHCHIP  // uncomment line when programming devices with 4K min flash = ATtiny 404, 414, 424, 804, 814, 824, 1604, 1614, 1624

// uncomment one of the following for PCB Revision. PCB v0.1 and v0.2 not supported
// #define PCBV0_3_4 // PCB Rev 0.3 or 0.4
// #define PCBV0_5   // PCB Rev 0.5
#define PCBV0_6_7  // PCB Rev 0.6 or 0.7

// Compile with Arduino 1.8.19 or later.
// Install MegaTinyCore via the Boards Manager
// MegaTinyCore 2.5.11
// Note: Optiboot bootloader not used
//  Board: ATtiny 3224/1624/1614/1604/814/804/424/414/404/214/204
//  Chip or Board: Select your chip
//  Clock: 20MHz internal
//  millis()/micros() TCB0 (breaks tone() and Servo)
//  Startup Time: 8ms
//  Printf: Default
//  Wire: Master or Slave (saves flash & RAM
//
// The following settings are irrelevant as we are not using bootloader/Optiboot
//  BOD Voltage level: Any, BOD not used without bootloader
//  BOD Mode when Active/Sleeping: Any, BOD not used without bootloader
//  Save EEPROM: Any, not used without bootloader
//  UPDI/Reset Pin Function: Any, not used without bootloader

#ifdef MILLIS_USE_TIMERA0
#  error "This sketch takes over TCA0 - please use a different timer for millis()/micros() timer"
#endif

#ifndef MILLIS_USE_TIMERB0
#  error "This sketch is written for use with TCB0 as the millis() timer"
#endif

#define ledOn LOW  // led circuits designed so that grounding the pin turns the LED on
#define ledOff HIGH

#define buzzerOn HIGH  // buzzer circuit designed so that setting pin high turns the buzzer on
#define buzzerOff LOW

#define fanOn HIGH  // fan circuit designed so that setting pin high turns the buzzer on
#define fanOff LOW
#define fanOffPWMValue 0

#ifdef PCBV0_3_4
// Pin connections per PAPR V0.3 PCB OR v0.4
// PA0 - UPDI/RESET
// PA1 - ADC - BATTERY
// PA2 - ADC = POTENTIOMETER
// PA3 - FAN SENSE - not used
// PA4 - EXT PWR SENSE - not used
// PA5 - LED1
// PA6 - LED2
// PA7 - LED3
// PB0 - FAN PWM
// PB1 - LED4
// PB2 - UART TX
// PB3 - UART RX

int analogBatt = PIN_A1;
int analogPot  = PIN_A2;
int pinFanPWM  = PIN_PB0;
int pinLED1    = PIN_PA5;
int pinLED2    = PIN_PA6;
int pinLED3    = PIN_PA7;
int pinLED4    = PIN_PB0;
#endif  // PCBV0_3_4

#ifdef PCBV0_5
// Pin connections per PAPR V0.5
// PA0 - UPDI/RESET
// PA1 - ADC - BATTERY
// PA2 - ADC = POTENTIOMETER
// PA3 - NC
// PA4 - LED4
// PA5 - LED1
// PA6 - LED2
// PA7 - LED3
// PB0 - FAN PWM
// PB1 - BUZZER
// PB2 - UART TX
// PB3 - UART RX

int analogBatt = PIN_A1;
int analogPot  = PIN_A2;
int pinFanPWM  = PIN_PB0;
int pinLED1    = PIN_PA5;
int pinLED2    = PIN_PA6;
int pinLED3    = PIN_PA7;
int pinLED4    = PIN_PB0;
int pinBuzzer  = PIN_PB1;
#endif

#ifdef PCBV0_6_7
// Pin connections per PAPR V0.6 & V0.7 PCB
// PA0 - UPDI/RESET
// PA1 - ADC - FUSE-ILM - future use
// PA2 - ADC - POTENTIOMETER
// PA3 - ADC - BATTERY
// PA4 - LED1
// PA5 - LED2
// PA6 - LED3
// PA7 - LED4
// PB0 - FAN PWM
// PB1 - BUZZER
// PB2 - DEBUG TX
// PB3 - FUSE-PG

int analogFuseIlm = PIN_A1;     // AIN1 - For measuring the output current
                                // Iout(A) = Vilm(uV) / [ Rilm(ohms) * Gimon(uA/A) ]
                                // Rilm = R17 = 1000ohms
                                // Gimon = 182 uA/A
int analogPot        = PIN_A2;  // AIN2 - Fan speed control POT with on/off switch
int analogBatt       = PIN_A3;  // AIN3 - 10 bit resolution on ADC
int pinLED1          = PIN_PA4;
int pinLED2          = PIN_PA5;
int pinLED3          = PIN_PA6;
int pinLED4          = PIN_PA7;
int pinFanPWM        = PIN_PB0;  // W00 8 bit resolution with Arduino AnalogWrite (v0.3 change)
int pinBuzzer        = PIN_PB1;
int pinFusePowerGood = PIN_PB3;  // Active high when power is good
#endif                           // PCBV0_6_7

// Battery Voltage to Fuel Gauge
// > 4.12V/Cell = 100% - 78% Battery  = 4 LEDs         => 899 > adc > 864
// > 3.95V/Cell =  77% - 55% Battery  = 3 LEDs         => 864 > adc > 816
// > 3.70V/Cell =  54% - 33% Battery  = 2 LEDs         => 816 > adc > 781
// > 3.54V/Cell =  32% - 10% Battery  = 1 LEDs         => 781 > adc > 712
// > 3.25V/Cell =  10% - 0%  Battery  = LEDS FLASHING  => 712 > adc > 618
// > 2.80V/Cell =  0%        Battery  = Shut Down

const int battADCMax  = 1023;
const int battADCFull = 899;
const int battADC78p  = 864;
const int battADC55p  = 816;
const int battADC33p  = 781;
const int battADC10p  = 712;
const int battADC0p   = 618;
const int battADCMin  = 0;

// Min PWM mapping for blower:
// Limits the min pot value     Voltage   Min PWM Percentage
const int minPWMfull = 50;   // 11.85V      69     >77%
const int minPWM75p  = 55;   // 11.10V      72     >54%
const int minPWM50p  = 61;   // 10.62V      77     >33%
const int minPWM25p  = 70;   // 9.75V       88     >10%
const int minPWM10p  = 126;  // 8.40V      110     >0%
// These values were determined empirically by adjusting the input voltage and dialing the PWM down until motor stall

// State Machine states
enum batteryStates { undefined, batteryDead, battery10p, battery25p, battery50p, battery75p, batteryFull, batteryCheck };

uint8_t batteryState = batteryCheck;

const uint8_t lowBatteryLoopCounts = 25;    // decrease for 10% batteryADCSampleAverage LED to blink faster
const uint16_t numBatterySamples   = 10;    // number of samples to average
const uint16_t maxADCValue10bit    = 1023;  // maximum value of 10bit reading
const uint16_t minADCValue10bit    = 0;     // minimum value of 10bit reading

uint8_t ledDanceOffTime        = 5;   // 5 ms
uint8_t ledDanceOnTime         = 95;  // 95 ms
uint8_t lowBatteryBlinkCounter = 0;   // counter used for low batteryADCSampleAverage blink

uint16_t batteryADCSampleAverage = 1023;
uint16_t rawADC                  = 0;
uint16_t vILM                    = 0;
uint8_t fanPWM                   = 0;
uint8_t minPWM                   = minPWM10p;
uint8_t maxPWM                   = 255;

void setLEDs( bool led1State, bool led2State, bool led3State, bool led4State ) {
  digitalWriteFast( pinLED1, led1State );
  digitalWriteFast( pinLED2, led2State );
  digitalWriteFast( pinLED3, led3State );
  digitalWriteFast( pinLED4, led4State );
}

void setBuzzer( bool buzzerStateToSet ) {
#ifndef PCBV0_3_4
  digitalWriteFast( pinBuzzer, buzzerStateToSet );
#endif
}

void checkPowerGood() {
#ifdef PCBV0_6_7
  if( digitalReadFast( pinFusePowerGood ) == HIGH ) {
    setBuzzer( true );
  }
  else {
    setBuzzer( false );
  }
#endif
}

void setFan( uint8_t fanPWMValue ) {
  if( fanPWMValue == 255 ) {
    digitalWriteFast( pinFanPWM, HIGH );  // Turn on fan 100%
  }
  else if( fanPWMValue == fanOffPWMValue ) {
    digitalWriteFast( pinFanPWM, LOW );  // Turn on fan 100%
  }
  else {
    analogWrite( pinFanPWM, fanPWMValue );
  }
}

// the setup routine runs once when you press reset:
void setup() {
  // Timer for PWM
  //   See http://ww1.microchip.com/downloads/en/Appnotes/TB3217-Getting-Started-with-TCA-DS90003217.pdf
  TCA0.SINGLE.CTRLB = ( TCA_SINGLE_CMP2EN_bm | TCA_SINGLE_WGMODE_SINGLESLOPE_gc );  // Single PWM on WO0 singleslope
  TCA0.SINGLE.PER   = 0xFF;                                                         // Count down from 255 on WO0/WO1/WO2
  TCA0.SINGLE.CTRLA = ( TCA_SINGLE_CLKSEL_DIV4_gc | TCA_SINGLE_ENABLE_bm );         // enable the timer with prescaler of 4

  // Analog
  analogReference( VDD );      // Default reference is Vdd
  analogReadResolution( 10 );  // Default resolution is 10 bits
  analogSampleDuration( 30 );  // Increase the ADC SAMPLEDUR to filter out some noise

  // GPIO
  pinMode( pinLED1, OUTPUT );
  pinMode( pinLED2, OUTPUT );
  pinMode( pinLED3, OUTPUT );
  pinMode( pinLED4, OUTPUT );
  pinMode( pinFanPWM, OUTPUT );  // PB0 - TCA0 WO0, pin9 on 14-pin parts
#ifndef PCBV0_3_4
  pinMode( pinBuzzer, OUTPUT );
#endif
#ifdef PCBV0_6_7
  pinMode( pinFusePowerGood, INPUT );
#endif
  setLEDs( ledOff, ledOff, ledOff, ledOff );
  setBuzzer( false );
  setFan( maxPWM );

#ifdef TWOKFLASHCHIP
  delay( 1000 );
#else
  Serial.begin( 115200 );
  Serial.println( "Starting up" );
  Serial.println( "PAPRA 12JUNE2022" );
#  if defined( PCBV0_3_4 )
  Serial.println( "PCB v0.3/v0.4" );
#  elif defined( PCBV0_5 )
  Serial.println( "PCB v0.5" );
#  elif defined( PCBV0_6_7 )
  Serial.println( "PCB v0.6/v0.7" );
#  endif
  Serial.println( "AtTiny 0/1/2-Series 14 pin" );
  Serial.println( "Tetra Bio Distributed 2022" );
  for( int i = 0; i <= 4; i++ ) {  // startup led dance
    setLEDs( ledOn, ledOff, ledOff, ledOff );
    delay( ledDanceOnTime );
    setLEDs( ledOff, ledOn, ledOff, ledOff );
    delay( ledDanceOnTime );
    setLEDs( ledOff, ledOff, ledOn, ledOff );
    delay( ledDanceOnTime );
    setLEDs( ledOff, ledOff, ledOff, ledOn );
    delay( ledDanceOnTime * 2 );
    setLEDs( ledOff, ledOff, ledOn, ledOff );
    delay( ledDanceOnTime );
    setLEDs( ledOff, ledOn, ledOff, ledOff );
    delay( ledDanceOnTime );
    setLEDs( ledOn, ledOff, ledOff, ledOff );
    delay( ledDanceOnTime );
    setLEDs( ledOff, ledOff, ledOff, ledOff );
    delay( ledDanceOffTime );
  }
#endif
}

// the loop routine runs over and over again forever:
void loop() {
  delay( 25 );
  checkPowerGood();
  // Clamp and rescale potentiometer input from a 10bit value to 8 bit
  if( maxPWM > fanOffPWMValue ) {
    rawADC = analogRead( analogPot );
    fanPWM = map( rawADC, minADCValue10bit, maxADCValue10bit, minPWM, maxPWM );  // scale knob, 10 bits to 8 bits
  }
  else {
    fanPWM = fanOffPWMValue;
  }
  batteryADCSampleAverage = ( ( batteryADCSampleAverage * ( numBatterySamples - 1 ) ) + analogRead( analogBatt ) ) / numBatterySamples;
  switch( batteryADCSampleAverage ) {
    case battADC78p ... battADCMax:  // Battery Full
      if( batteryState > batteryFull ) {
        batteryState = batteryFull;
        setLEDs( ledOn, ledOff, ledOff, ledOff );  // LED 1 ON
        minPWM = minPWMfull;
      }
      break;
    case battADC55p ...( battADC78p - 1 ):  // Battery 75%
      if( batteryState > battery75p ) {
        batteryState = battery75p;
        setLEDs( ledOff, ledOn, ledOff, ledOff );  // LED 2 ON
        minPWM = minPWM75p;
      }
      break;
    case battADC33p ...( battADC55p - 1 ):  // Battery 50%
      if( batteryState > battery50p ) {
        batteryState = battery50p;
        setLEDs( ledOff, ledOff, ledOn, ledOff );  // LED 3 ON
        minPWM = minPWM50p;
      }
      break;
    case battADC10p ...( battADC33p - 1 ):  // Battery 25%
      if( batteryState > battery25p ) {
        batteryState = battery25p;
        setLEDs( ledOff, ledOff, ledOff, ledOn );  // LED 1 ON
        minPWM = minPWM25p;
      }
      break;
    case battADC0p ...( battADC10p - 1 ):  // Battery 10% - blink LED1
      if( batteryState > battery10p ) {
        batteryState   = battery10p;
        bool led1State = digitalReadFast( pinLED1 );
        if( lowBatteryBlinkCounter++ > lowBatteryLoopCounts ) {
          // For batteryADCSampleAverage state of 0-10%, blink LED1 to indicate almost dead batteryADCSampleAverage
          led1State              = !led1State;
          lowBatteryBlinkCounter = 0;
        }
        setLEDs( ledOff, ledOff, ledOff, led1State );  // LED 1 blinking
        setBuzzer( true );
        minPWM = minPWM10p;
      }
      break;
    case battADCMin ...( battADC0p - 1 ):  // Battery depleted - Shutdown
      if( batteryState > batteryDead ) {
        batteryState = batteryDead;
        setLEDs( ledOff, ledOff, ledOff, ledOff );  // LEDs off
        maxPWM = fanOffPWMValue;                    // turn off fan - overrides the prior value
      }
      break;
  }
  setFan( fanPWM );

#ifndef TWOKFLASHCHIP
  Serial.print( " Battery = " );
  Serial.print( batteryADCSampleAverage );
  Serial.print( " rawADC = " );
  Serial.print( rawADC );
  Serial.print( " pwm = " );
  Serial.print( fanPWM );
  Serial.print( " Battery State = " );
  Serial.println( batteryState );
#endif
}
