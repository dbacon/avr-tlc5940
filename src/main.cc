//
//   Copyright 2012 Dave Bacon
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//

#include <avr/io.h>

#define __devboard_mine__ 1

#if defined(ARDUINO)
#  define __led13_support
#  define delay_100us(x) delay(x)
#elif defined(__devboard_mine__)
# include "delay.h"
#else
#  warning "no target board defined"
#endif

#define pbcl(port, b) do { port &= (uint8_t)~(_BV(b)); } while (0)
#define pbst(port, b) do { port |= _BV(b); } while (0)

// TLC5940 minimum pulse durations (16ns & 20ns) are smaller than the fastest a 16mHz AVR can do (125ns)
#define pulse(port, b) do { pbst(port, b); pbcl(port, b); } while (0);


#ifdef __led13_support
#  define LEDDDR DDRB
#  define LEDPORT PORTB
#  define LEDPIN 5

static void led13_init() {
	  pbst(LEDDDR, LEDPIN);
}

// useful for visual or logic analyzer identification
static void flashled(int count, int hdelay) {
  for (int i = 0; i < count; ++i) {
    pbst(LEDPORT, LEDPIN);
    delay_100us(hdelay);
    pbcl(LEDPORT, LEDPIN);
    delay_100us(hdelay);
  }
}

#else
#  define led13_init() while(0);
#  define flashled(count, delay) while(0);
#endif // __led13_support


#if defined(ARDUINO)
#  define TLC_PORT PORTB
#  define TLC_DDR DDRB
#elif defined(__devboard_mine__)
#  define TLC_PORT PORTA
#  define TLC_DDR DDRA
#else
#  warning "no target board defined"
#endif

#define TLC_0_SIN   0
#define TLC_1_SCLK  1
#define TLC_2_XLAT  2
#define TLC_3_BLANK 3
#define TLC_4_GSCLK 4
#define TLC_5_SOUT  xx
#define TLC_6_XERR  xx

#if defined(ARDUINO)
#  define ROWSEL_DDR DDRD
#  define ROWSEL_PORT PORTD
#elif defined(__devboard_mine__)
#  define ROWSEL_DDR DDRC
#  define ROWSEL_PORT PORTC
#else
#  warning "no target board defined"
#endif

#define GSBPP 12
#define GSPWMTOP 0xfff
#define ROWCOUNT 8
#define PIXELSPERROW 16

static int8_t pci = -1;
static int8_t btci = GSBPP-1;
static uint16_t pixel = 0;
static uint8_t rowi = 4;
static uint16_t pwmi = 0;


#define FACTOR 128

uint16_t gsdata[][16] = {

  {
    FACTOR*7, FACTOR*6, FACTOR*5, FACTOR*4, FACTOR*3, FACTOR*2, FACTOR*1, FACTOR*0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  },

  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    FACTOR*0, FACTOR*1, FACTOR*2, FACTOR*3, FACTOR*4, FACTOR*5, FACTOR*6, FACTOR*7,
  }

};

void setup() {

  led13_init();

  // row selectors all outputs
  ROWSEL_DDR = 0xff;


  // row data controller (tlc5940) data loading serial interface
  TLC_DDR |= _BV(TLC_0_SIN) | _BV(TLC_1_SCLK) | _BV(TLC_2_XLAT);

  // row data controller (tlc5940) pwm controller
  TLC_DDR |= _BV(TLC_3_BLANK) | _BV(TLC_4_GSCLK);


  ROWSEL_PORT = 0xff; // active low, all disabled at start

  pbst(TLC_PORT, TLC_3_BLANK); // blank stuff until we setup

  // row data controller pins are active high
  TLC_PORT &= (uint8_t)~(_BV(TLC_0_SIN) | _BV(TLC_1_SCLK) | _BV(TLC_2_XLAT) | _BV(TLC_4_GSCLK));

  flashled(5, 20);

  for (int8_t i = PIXELSPERROW; i >= 0; --i) {

    uint16_t q = gsdata[0][i];

    for (uint8_t b = 0; b < GSBPP; ++b) {
      if ((q & (1<<(GSBPP-1))) != 0) {
        pbst(TLC_PORT, TLC_0_SIN);
      } else {
        pbcl(TLC_PORT, TLC_0_SIN);
      }

      pulse(TLC_PORT, TLC_1_SCLK);

      q<<=1;
    }
  }

  pulse(TLC_PORT, TLC_2_XLAT);

  pbcl(TLC_PORT, TLC_3_BLANK);

}

void loop() {

  // we never return from the 1st call to loop()

  pwmi = GSPWMTOP;

  while (1) {

    // TODO: offload this to SPI hardware
    if (pci >= 0) {

      if ((pixel & (1<<(GSBPP-1))) != 0) {
        pbst(TLC_PORT, TLC_0_SIN);
      } else {
        pbcl(TLC_PORT, TLC_0_SIN);
      }
      pulse(TLC_PORT, TLC_1_SCLK);

      pixel <<= 1;
      btci--;

      if (btci < 0) {
        pci--;
        pixel = gsdata[rowi%2][pci];
        btci = GSBPP-1;
      }
    }

    pulse(TLC_PORT, TLC_4_GSCLK);

    if (pwmi == 0) {
      pwmi = GSPWMTOP;

      // new GS data has been shifted, latch it
      pulse(TLC_PORT, TLC_2_XLAT);

      // reset GS counter
      pulse(TLC_PORT, TLC_3_BLANK);

      rowi = (rowi + 1) % ROWCOUNT;

      pci = PIXELSPERROW - 1;
      pixel = gsdata[rowi%2][pci];

      ROWSEL_PORT = ~(1<<rowi);

    } else {
      pwmi--;
    }
  }
}

#if !defined(ARDUINO)

int main(int argc, char** argv) {
	setup();
	loop();
}

#endif
