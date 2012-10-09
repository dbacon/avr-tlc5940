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
#include <avr/interrupt.h>
#include <avr/wdt.h>

#define __devboard_mine__ 1

#define pbcl(port, b) do { port &= (uint8_t)~(_BV(b)); } while (0)
#define pbst(port, b) do { port |= _BV(b); } while (0)

// TLC5940 minimum pulse durations (16ns & 20ns) are smaller than the fastest a 16mHz AVR can do (125ns)
#define pulse(port, b) do { pbst(port, b); pbcl(port, b); } while (0);


#if defined(ARDUINO)
#  define TLC_PORT PORTB
#  define TLC_DDR DDRB
#  define TLC_0_GSCLK           0
#  define TLC_1_XLAT            1 // latches data sent (not the same as SPI SS' going high)
#  define TLC_2_BLANK           2 // resets GS counter (conflicts SPI SS', but we are not using it)
#  define TLC_3_SIN___SPI_MOSI  3
#  define TLC_4_SOUT__SPI_MISO  4
#  define TLC_5_SCLK__SPI_SCK   5
#elif defined(__devboard_mine__)
#  define TLC_PORT PORTB
#  define TLC_DDR DDRB
#  define TLC_0_GSCLK           1 // align with CLKO
#  define TLC_1_XLAT            3 // latches data sent (not the same as SPI SS' going high)
#  define TLC_2_BLANK           4 // resets GS counter (conflicts SPI SS', but we are not using it)
#  define TLC_3_SIN___SPI_MOSI  5
#  define TLC_4_SOUT__SPI_MISO  6
#  define TLC_5_SCLK__SPI_SCK   7
#else
#  warning "no target board defined"
#endif

#define SPI_DD     TLC_DDR
#define SPI_MOSI   TLC_3_SIN___SPI_MOSI
#define SPI_SCK    TLC_5_SCLK__SPI_SCK

#if defined(ARDUINO)
#  define ROWSEL_DDR DDRD
#  define ROWSEL_PORT PORTD
#elif defined(__devboard_mine__)
#  define ROWSEL_DDR DDRC
#  define ROWSEL_PORT PORTC
#else
#  warning "no target board defined"
#endif

#define ROWCOUNT 8

static volatile int8_t pci = -1;
static volatile uint8_t rowi = 4;


#define FACTOR 1

#define DATAROWS 8

uint16_t gsdataf[][DATAROWS][2] = {
    { { 0x000, 7*FACTOR, }, { 0x000, 6*FACTOR, }, { 0x000, 5*FACTOR, }, { 0x000, 4*FACTOR, }, { 0x000, 3*FACTOR, }, { 0x000, 2*FACTOR, }, { 0x000, 1*FACTOR, }, { 0x000, 0*FACTOR, }, },
    { { 0x000, 7*FACTOR, }, { 0x000, 6*FACTOR, }, { 0x000, 5*FACTOR, }, { 0x000, 4*FACTOR, }, { 0x000, 3*FACTOR, }, { 0x000, 2*FACTOR, }, { 0x000, 1*FACTOR, }, { 0x000, 0*FACTOR, }, },
    { { 0*FACTOR, 0x000, }, { 1*FACTOR, 0x000, }, { 2*FACTOR, 0x000, }, { 3*FACTOR, 0x000, }, { 4*FACTOR, 0x000, }, { 5*FACTOR, 0x000, }, { 6*FACTOR, 0x000, }, { 7*FACTOR, 0x000, }, },
    { { 0*FACTOR, 0x000, }, { 1*FACTOR, 0x000, }, { 2*FACTOR, 0x000, }, { 3*FACTOR, 0x000, }, { 4*FACTOR, 0x000, }, { 5*FACTOR, 0x000, }, { 6*FACTOR, 0x000, }, { 7*FACTOR, 0x000, }, },
    { { 0x000, 7*FACTOR, }, { 0x000, 6*FACTOR, }, { 0x000, 5*FACTOR, }, { 0x000, 4*FACTOR, }, { 0x000, 3*FACTOR, }, { 0x000, 2*FACTOR, }, { 0x000, 1*FACTOR, }, { 0x000, 0*FACTOR, }, },
    { { 0x000, 7*FACTOR, }, { 0x000, 6*FACTOR, }, { 0x000, 5*FACTOR, }, { 0x000, 4*FACTOR, }, { 0x000, 3*FACTOR, }, { 0x000, 2*FACTOR, }, { 0x000, 1*FACTOR, }, { 0x000, 0*FACTOR, }, },
    { { 0*FACTOR, 0x000, }, { 1*FACTOR, 0x000, }, { 2*FACTOR, 0x000, }, { 3*FACTOR, 0x000, }, { 4*FACTOR, 0x000, }, { 5*FACTOR, 0x000, }, { 6*FACTOR, 0x000, }, { 7*FACTOR, 0x000, }, },
    { { 0*FACTOR, 0x000, }, { 1*FACTOR, 0x000, }, { 2*FACTOR, 0x000, }, { 3*FACTOR, 0x000, }, { 4*FACTOR, 0x000, }, { 5*FACTOR, 0x000, }, { 6*FACTOR, 0x000, }, { 7*FACTOR, 0x000, }, },
};

uint8_t gsdata[DATAROWS][24] = { };

static void convert16to12packed(uint16_t scale, int8_t orientation) {
  for (uint8_t r = 0; r < DATAROWS; ++r) {

    for (uint8_t ppi = 0; ppi < 4; ++ppi) {

      /*

      ppi = 0:
      0 -> 0 h  0 m
      1 -> 0 l  1 h
      2 -> 1 m  1 l

      ppi = 1:
      3 -> 2 h  2 m
      4 -> 2 l  3 h
      5 -> 3 m  3 l

      ...

      */

	  uint8_t col1 = ppi*2+0;
	  uint8_t col2 = ppi*2+1;

      if (orientation) {
		  col1 = 8-col1-1;
		  col2 = 8-col2-1;
      }

      uint16_t pv1;
      uint16_t pv2;


      // green, (msb 1st)

      pv1 = gsdataf[r][col1][1] * scale;
      pv2 = gsdataf[r][col2][1] * scale;

      gsdata[r][ 0 + ppi*3 + 0] = (uint8_t)(((pv1 & 0xff0) >> 4)                        );
      gsdata[r][ 0 + ppi*3 + 1] = (uint8_t)(((pv1 & 0x00f) << 4) | ((pv2 & 0xf00) >> 8) );
      gsdata[r][ 0 + ppi*3 + 2] = (uint8_t)(                       ((pv2 & 0x0ff) >> 0) );


      // red

      pv1 = gsdataf[r][col1][0] * scale;
      pv2 = gsdataf[r][col2][0] * scale;

      gsdata[r][12 + ppi*3 + 0] = (uint8_t)(((pv1 & 0xff0) >> 4)                        );
      gsdata[r][12 + ppi*3 + 1] = (uint8_t)(((pv1 & 0x00f) << 4) | ((pv2 & 0xf00) >> 8) );
      gsdata[r][12 + ppi*3 + 2] = (uint8_t)(                       ((pv2 & 0x0ff) >> 0) );

    }
  }
}

void setup() {

  // row selectors all outputs
  ROWSEL_DDR = 0xff;


  // row data controller (tlc5940) data loading serial interface
  TLC_DDR |= _BV(TLC_3_SIN___SPI_MOSI) | _BV(TLC_5_SCLK__SPI_SCK) | _BV(TLC_1_XLAT);

  // row data controller (tlc5940) pwm controller
  TLC_DDR |= _BV(TLC_2_BLANK) | _BV(TLC_0_GSCLK);


  ROWSEL_PORT = 0xff; // active low, all disabled at start

  pbst(TLC_PORT, TLC_2_BLANK); // blank stuff until we setup

  // row data controller pins are active high
  TLC_PORT &= (uint8_t)~(_BV(TLC_3_SIN___SPI_MOSI) | _BV(TLC_5_SCLK__SPI_SCK) | _BV(TLC_1_XLAT) | _BV(TLC_0_GSCLK));


  // set mosi sck as output // already done by TLC setup
  // SPI_DD |= _BV(SPI_MOSI) | _BV(SPI_SCK);

  // SPI enabled, MSB first data order, master mode, normal clock polarity & phase, maximum clock rate, no 2x, interrupt mode
  SPCR = _BV(MSTR) | _BV(SPE) | _BV(SPIE);

  convert16to12packed(127, 0);

  // allow 1st GS cycle to begin
  pbcl(TLC_PORT, TLC_2_BLANK);


  // OC0A not connected, OC0B not connected, CTC (clear timer on compare match) waveform generation mode
  TCCR0A = _BV(WGM01);
  // timer clock is i/o clk / 1024
  TCCR0B = _BV(CS02) | _BV(CS00);
  // set TOP to 3, so 4096 i/o clocks are counted between our interrupts
  OCR0A = 3;
  // enable interrupt from compare match on channel A
  TIMSK0 = _BV(OCIE0A);

}

ISR(TIMER0_COMPA_vect) {

  pbst(TLC_PORT, TLC_2_BLANK);

  pulse(TLC_PORT, TLC_1_XLAT); // latch the data we've sent for the next row
  ROWSEL_PORT = ~(1<<rowi);    // enable the row we shifted data for

  rowi = (rowi + 1) % ROWCOUNT;

  pci = 24 - 1; // 16 * 12 / 8
  SPDR = gsdata[rowi%DATAROWS][24-pci-1]; // initiate SPI xfer of next row's data

  // reset GS counter
  pbcl(TLC_PORT, TLC_2_BLANK);

}

ISR(SPI_STC_vect) {

  pci--;

  if (pci >= 0) {
    SPDR = gsdata[rowi%DATAROWS][24-pci-1];
  }
}


void loop() {

  // we never return from the 1st call to loop()

  uint16_t scale = 1;
  int8_t dir = 1;
  int8_t orientation = 0;

  while (1) {

    convert16to12packed(scale, orientation);

    scale += dir;
    if (scale >= 510) {
    	dir = -1;
    }
    if (scale <= 2) {
    	dir = 1;
    	orientation = !orientation;
    }
  }
}


uint8_t resetflags __attribute__((section(".noinit")));

void save_and_clear_mcusr()
	__attribute__((section(".init3")))
	__attribute__((naked));

void save_and_clear_mcusr() {
	resetflags = MCUSR;
	MCUSR = 0;
	wdt_disable();
}


#if !defined(ARDUINO)

int main(int argc, char** argv) {

  setup();

  sei();

  loop();
}

#endif
