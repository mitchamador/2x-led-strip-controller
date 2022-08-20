/*******************************************************
Chip type               : ATtiny13A
AVR Core Clock frequency: 9,600000 MHz
Memory model            : Tiny
External RAM size       : 0
Data Stack size         : 16
*******************************************************/

// FUSES high - 0xFF low - 0x7A

// #include <tiny13a.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>

// single channel pwm
//#define SINGLE_CHANNEL

// Declare your global variables here
#define KEY           (!(PINB & _BV(PB2)))
#define PWM_A PB0
#ifndef SINGLE_CHANNEL
#define PWM_B PB1
#endif

#define _SET(bit)      PORTB |= (1 << bit)
#define _CLEAR(bit)    PORTB &= ~(1 << bit)

// prescaler: 1,8,64 (PWM frequencies (@1.2MHz): 4.687kHz, 585Hz, 73Hz)
#define TIMER_PRESCALER 8
#define TIMER_RESOLUTION (256.0 * TIMER_PRESCALER / F_CPU)
#define TIMER_RESOLUTION_100 (25600 * TIMER_PRESCALER / F_CPU)

#if (TIMER_RESOLUTION_100 >= 1)
#define SKIP_KEY_TIMER_DELAY
#define KEY_TIMER_RESOLUTION TIMER_RESOLUTION
#else
#define KEY_TIMER_DELAY ((uint8_t) (0.01 / TIMER_RESOLUTION))
#define KEY_TIMER_RESOLUTION 0.01
#endif

// длинное нажатие (> 0,5 с)
#define LONG_KEYPRESS ((uint16_t) (0.5 / KEY_TIMER_RESOLUTION))

// короткое нажатие (> 0,04 с)
#define KEYPRESS ((uint16_t) (0.04 / KEY_TIMER_RESOLUTION))

#ifndef SINGLE_CHANNEL
// длительность двойного нажатия (в пределах 0,3 с)
#define DOUBLECLICK_DELAY ((uint16_t) (0.3 / KEY_TIMER_RESOLUTION))
#endif

// задержка изменения PWM (вкл/выкл)
#define PWM_DELAY          ((uint16_t) (3.0 / (256 * TIMER_RESOLUTION)))
#define PWM_DELAY_OFF      ((uint16_t) (7.5 / (256 * TIMER_RESOLUTION)))

// min brightness
#define PWM_MIN 50
// max brigthness
#define PWM_MAX 255

#ifdef SINGLE_CHANNEL

#define SET_PWM_A(x)                                                                          \
OCR0A = x;                                                                                    \
if (x == 0) {                                                                                 \
    TCCR0A = (0<<COM0B1) | (0<<COM0B0) | (0<<COM0A1) | (0<<COM0A0) | (1<<WGM01) | (1<<WGM00); \
    _CLEAR(PWM_A);                                                                            \
} else {                                                                                      \
    TCCR0A = (0<<COM0B1) | (0<<COM0B0) | (1<<COM0A1) | (0<<COM0A0) | (1<<WGM01) | (1<<WGM00); \
}                                                                                             \

#else

#define MASK_PWM_A ((1<<COM0A1) | (1<<COM0A0))
#define MASK_PWM_B ((1<<COM0B1) | (1<<COM0B0))

#define SET_PWM_A(x)                                                                        \
OCR0A = x;                                                                                  \
if (x == 0 || (lamp_mode & 0x01) == 0) {                                                    \
    TCCR0A = (TCCR0A & MASK_PWM_B) | (0<<COM0A1) | (0<<COM0A0) | (1<<WGM01) | (1<<WGM00);   \
    _CLEAR(PWM_A);                                                                          \
} else {                                                                                    \
    TCCR0A = (TCCR0A & MASK_PWM_B) | (1<<COM0A1) | (0<<COM0A0) | (1<<WGM01) | (1<<WGM00);   \
}                                                                                           \

#define SET_PWM_B(x)                                                                        \
OCR0B = x;                                                                                  \
if (x == 0 || (lamp_mode & 0x02) == 0) {                                                    \
    TCCR0A = (TCCR0A & MASK_PWM_A) | (0<<COM0B1) | (0<<COM0B0) | (1<<WGM01) | (1<<WGM00);   \
    _CLEAR(PWM_B);                                                                          \
} else {                                                                                    \
    TCCR0A = (TCCR0A & MASK_PWM_A) | (1<<COM0B1) | (0<<COM0B0) | (1<<WGM01) | (1<<WGM00);   \
}                                                                                           \

#endif

#ifndef SINGLE_CHANNEL
uint8_t lamp_mode;
#endif

typedef enum
{
  MODE_IDLE = 0,
  MODE_ON,
  MODE_OFF,
  MODE_BRIGHTNESS,
} mode_e;

mode_e mode;

// target pwm value
uint8_t pwm;

// 0 - up, ~0 - down
uint8_t brightness_direction;

//no presets (single click, longpress, doubleclick)
//#define NO_PRESETS

#ifdef NO_PRESETS
EEMEM uint8_t e_brightness = 0xFF, e_lamp_mode = 0x03;
#else
typedef enum
{
  PRESET_CLICK = 0,
  PRESET_LONGPRESS,
  PRESET_DOUBLECLICK,
} preset_e;

preset_e preset;

typedef struct
{
  uint8_t brightness;
  uint8_t lamp_mode;
} eemem_preset_t;

EEMEM eemem_preset_t e_preset[] = {
  {0xFF, 0x03},
  {0x32, 0x02},
  {0xFF, 0x01} 
};
#endif

// Timer 0 overflow interrupt service routine
ISR(TIM0_OVF_vect, ISR_NAKED)
{
  static uint8_t key_pressed_counter = 0;
#ifndef SKIP_KEY_TIMER_DELAY
  static uint8_t key_timer_delay_counter = 0;
#endif
  static uint8_t pwm_delay_counter = 0;

#ifndef SINGLE_CHANNEL
  static uint8_t doubleclick_counter = 0;
  static uint8_t singleclick = 0;
#endif

  if (pwm_delay_counter > 0) {
    pwm_delay_counter--;
  } else {
    if (mode == MODE_ON) {
      pwm_delay_counter = PWM_DELAY;
      if (pwm == 0) {
        // turn on
        // read pwm from eeprom
#ifdef NO_PRESETS
        pwm = eeprom_read_byte(&e_brightness);
#else
        pwm = eeprom_read_byte(&e_preset[preset].brightness);
#endif
        if (pwm < PWM_MIN) {
          pwm = PWM_MIN;
        }
        #ifndef SINGLE_CHANNEL
#ifdef NO_PRESETS
          lamp_mode = eeprom_read_byte(&e_lamp_mode);
#else
          lamp_mode = eeprom_read_byte(&e_preset[preset].lamp_mode);
#endif
          if (lamp_mode == 0) lamp_mode++;
        #endif
      }
    } else if (mode == MODE_OFF) {
      pwm_delay_counter = PWM_DELAY_OFF;
      pwm = 0;
    } else if (mode == MODE_BRIGHTNESS) {
      pwm_delay_counter = PWM_DELAY;
      if (brightness_direction == 0 && pwm < PWM_MAX) {
        pwm++;
      } else if (brightness_direction != 0 && pwm > PWM_MIN) {
        pwm--;
      } else {
        //brightness_direction = ~brightness_direction; // uncomment for cycling brightness
      }
    }

    //if (mode != MODE_IDLE)
    {
      unsigned _ocr0ab;
      _ocr0ab = OCR0A;
      if (OCR0A < pwm) {
        _ocr0ab = OCR0A + 1;
      } else if (OCR0A > pwm) {
        _ocr0ab = OCR0A - 1;
      } else if (mode != MODE_BRIGHTNESS && key_pressed_counter == 0) {
        mode = MODE_IDLE;
      }
      SET_PWM_A(_ocr0ab);
#ifndef SINGLE_CHANNEL
      SET_PWM_B(_ocr0ab);
#endif
    }
  }

#ifndef SKIP_KEY_TIMER_DELAY
  if (key_timer_delay_counter > 0) {
    key_timer_delay_counter--;
  } else {
    key_timer_delay_counter = (KEY_TIMER_DELAY + 1);
#else
  {
#endif

#ifndef SINGLE_CHANNEL
    if (doubleclick_counter > 0) {
      doubleclick_counter--;
      singleclick = doubleclick_counter == 0;
    }
#endif

    if (KEY) {
      if (key_pressed_counter >= LONG_KEYPRESS) {
        // длинное нажатие продолжается (изменение яркости)
        if (mode == MODE_IDLE && pwm != 0) {
          mode = MODE_BRIGHTNESS;
          if (pwm == PWM_MAX) {
            brightness_direction = ~0;
          } else if (pwm == PWM_MIN) {
            brightness_direction = 0;
          }
        } else if (mode == MODE_IDLE && pwm == 0) {
          // turn on
#ifndef NO_PRESETS
          preset = PRESET_LONGPRESS;
#endif
          mode = MODE_ON;
        }
      } else {
        key_pressed_counter++;
      }
    } else {
      if (key_pressed_counter >= LONG_KEYPRESS) {
        // длинное нажатие закончено (шаг яркости меняется на противоположный)
#ifndef SINGLE_CHANNEL
        singleclick = 0;
#endif
        if (mode == MODE_BRIGHTNESS) {
          mode = MODE_IDLE;
          brightness_direction = ~brightness_direction;

          // save brightness to eeprom
#ifdef NO_PRESETS
          eeprom_write_byte(&e_brightness, pwm);
#else
          eeprom_write_byte(&e_preset[preset].brightness, pwm);
#endif
        }
      } else if (key_pressed_counter >= KEYPRESS) {
#ifndef SINGLE_CHANNEL
        if (doubleclick_counter > 0) {
          // двойное нажатие с защитой от дребезга
          doubleclick_counter = 0;        
          // set lamp mode
          if (pwm != 0) {
            lamp_mode++;
            lamp_mode &= 0x03;
            if (lamp_mode == 0) lamp_mode++;
            // save lamp mode
#ifdef NO_PRESETS
            eeprom_write_byte(&e_lamp_mode, lamp_mode);
#else
            eeprom_write_byte(&e_preset[preset].lamp_mode, lamp_mode);
#endif
          } else if (mode == MODE_IDLE) {
            // turn on
#ifndef NO_PRESETS
            preset = PRESET_DOUBLECLICK;
#endif
            mode = MODE_ON;
          }
        } else {
          doubleclick_counter = DOUBLECLICK_DELAY;
        }
      } else if (singleclick) {
        // короткое нажатие с защитой от дребезга
        singleclick = 0;
#endif
        if (pwm == 0) {
          // turn on
#ifndef NO_PRESETS
          if (OCR0A == 0x00) {
            preset = PRESET_CLICK;
          }
#endif
          mode = MODE_ON;
        } else {
          // turn off
          mode = MODE_OFF;
        }
      }
      key_pressed_counter = 0;
    }
  }
  reti();
}

int main(void)
{
  // Declare your local variables here

  // Crystal Oscillator division factor: 1
  // CLKPR=(1<<CLKPCE);
  // CLKPR=(0<<CLKPCE) | (0<<CLKPS3) | (0<<CLKPS2) | (0<<CLKPS1) | (0<<CLKPS0);

  // Input/Output Ports initialization
  // Port B initialization
  // Function: Bit5=In Bit4=In Bit3=In Bit2=In Bit1=Out Bit0=Out 
  DDRB=(0<<DDB5) | (0<<DDB4) | (0<<DDB3) | (0<<DDB2) | (1<<DDB1) | (1<<DDB0);
  // State: Bit5=T Bit4=T Bit3=T Bit2=P Bit1=0 Bit0=0 
  PORTB=(0<<PORTB5) | (0<<PORTB4) | (0<<PORTB3) | (1<<PORTB2) | (0<<PORTB1) | (0<<PORTB0);

  // Timer/Counter 0 initialization
  // Clock source: System Clock
  // Clock value: 1200,000 kHz
  // Mode: Fast PWM top=0xFF
  // OC0A output: Non-Inverted PWM
  // OC0B output: Non-Inverted PWM
  // Timer Period: (256 * TIMER_PRESCALER / F_CPU)
  TCCR0A=(0<<COM0A1) | (0<<COM0A0) | (0<<COM0B1) | (0<<COM0B0) | (1<<WGM01) | (1<<WGM00);
  #if TIMER_PRESCALER == 1
  TCCR0B=(0<<WGM02) | (0<<CS02) | (0<<CS01) | (1<<CS00);
  #elif TIMER_PRESCALER == 8
  TCCR0B=(0<<WGM02) | (0<<CS02) | (1<<CS01) | (0<<CS00);
  #elif TIMER_PRESCALER == 64
  TCCR0B=(0<<WGM02) | (0<<CS02) | (1<<CS01) | (1<<CS00);
  #elif
  #error "prescaler not supported"
  #endif
  TCNT0=0x00;
  OCR0A=0x00;
  OCR0B=0x00;

  // Timer/Counter 0 Interrupt(s) initialization
  TIMSK0=(0<<OCIE0B) | (0<<OCIE0A) | (1<<TOIE0);

  // External Interrupt(s) initialization
  // INT0: Off
  // Interrupt on any change on pins PCINT0-5: Off
  GIMSK=(0<<INT0) | (0<<PCIE);
  MCUCR=(0<<ISC01) | (0<<ISC00);

  // Analog Comparator initialization
  // Analog Comparator: Off
  // The Analog Comparator's positive input is
  // connected to the AIN0 pin
  // The Analog Comparator's negative input is
  // connected to the AIN1 pin
  ACSR=(1<<ACD) | (0<<ACBG) | (0<<ACO) | (0<<ACI) | (0<<ACIE) | (0<<ACIS1) | (0<<ACIS0);
  ADCSRB=(0<<ACME);
  // Digital input buffer on AIN0: On
  // Digital input buffer on AIN1: On
  DIDR0=(0<<AIN0D) | (0<<AIN1D);

  // ADC initialization
  // ADC disabled
  ADCSRA=(0<<ADEN) | (0<<ADSC) | (0<<ADATE) | (0<<ADIF) | (0<<ADIE) | (0<<ADPS2) | (0<<ADPS1) | (0<<ADPS0);

  mode = MODE_IDLE;
  pwm = 0;

  // Global enable interrupts
  sei();

  while(1) {};
}
