/**
 * PawelBot servo controller for the Raspberry Pi.
 *
 * This program runs on an ATtiny85 connected to the RPi via the
 * I2C lines. It generates the PWM signals needed to control two
 * continuous turn servos. It also has an ADC input for checking
 * the battery level.
 *
 * To test on Linux:
 *
 * # # Assume that the ATtiny85's I2C pins are connected to the
 * # # host's I2C bus 1
 * # i2cdetect -y 1
 *      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
 * 00:          -- 04 -- -- -- -- -- -- -- -- -- -- --
 * 10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 70: -- -- -- -- -- -- -- --
 * # i2cget -y 1 4 0  ## get left PWM duration
 * 0x60
 * # i2cget -y 1 4 1  ## get right PWM duration
 * 0x60
 * # i2cget -y 1 4 2  ## get battery voltage/2 (0-255)
 * 0xbe
 * # i2cset -y 1 4 0 160 ## set left PWM duration
 * # i2cset -y 1 4 1 160 ## set right PWM duration
 *
 * Programming an ATtiny85 with an Arduino Uno
 *
 * 1. Untar tiny.tgz into your ~/sketchbook/hardware or whereever your
 *    Arduino sketchbook directory is. These files come from http://hlt.media.mit.edu/?p=1229,
 *    but they're not always available. As a bonus, I've already made
 *    a boards.txt that defines the right ATtiny85 configuration for this project. 
 * 2. In the Arduino IDE, program the ArduinoISP firmware onto the Uno 
 *    (it's an example Arduino program)
 * 3. In Arduino IDE, set the board to "ATtiny85 @ 8 MHz (internal oscillator; BOD disabled)
 * 4. Set the programmer to "Arduino as ISP"
 *
 *  -- Setup steps are done --
 *
 * 5. Wire the Uno to the ATtiny85 like this:
 *      Arduino +5V    ---> ATtiny Pin 8
 *      Arduino Ground ---> ATtiny Pin 4
 *      Arduino Pin 10 ---> ATtiny Pin 1
 *      Arduino Pin 11 ---> ATtiny Pin 5
 *      Arduino Pin 12 ---> ATtiny Pin 6
 *      Arduino Pin 13 ---> ATtiny Pin 7
 *      Arduino Reset, Arduino group -> connect with 10 uF capacitor
 * 5. Burn bootloader (VERY, VERY IMPORTANT. I learned the hard way.)
 * 6. Compile and upload this program.
 */


/**
 * ATtiny85 pin assignments
 *
 * Pin   Function
 * 1     NRESET
 * 2     PB3 - Battery level/2 * 3.3/255
 * 3     PB4 - Right servo
 * 4     GND
 * 5     PB0 - I2C SDA
 * 6     PB1 - Left servo
 * 7     PB2 - I2C SCL
 * 8     VCC (+3.3V)
 */

#define I2C_SLAVE_ADDRESS 0x4
#include <TinyWireS.h>

// The default buffer size, Can't recall the scope of defines right now
#ifndef TWI_RX_BUFFER_SIZE
#define TWI_RX_BUFFER_SIZE ( 16 )
#endif

// Tracks the current register pointer position
volatile byte reg_position;
const byte reg_count = 3;

/**
 * This is called for each read request we receive, never put more than one byte of data (with TinyWireS.send) to the
 * send-buffer when using this callback
 */
void requestEvent()
{
    byte value = 0;
    switch (reg_position) {
    case 0:
       value = OCR1A;
       break;
    case 1:
       value = OCR1B;
       break;
    case 2:
       value = byte(analogRead(3) >> 2);
       break;
    }
    TinyWireS.send(value);
    reg_position++;
    if (reg_position >= reg_count)
       reg_position = 0;
}

/**
 * The I2C data receive handler
 *
 * This needs to complete before the next incoming transaction (start, data, restart/stop) on the bus does
 * so be quick, set flags for long running tasks to be called from the mainloop instead of running them directly,
 */
void receiveEvent(uint8_t howMany)
{
    // Sanity check
    if (howMany < 1 || howMany > TWI_RX_BUFFER_SIZE)
      return;

    // Read register position
    reg_position = TinyWireS.receive();
    howMany--;

    if (reg_position >= reg_count) {
       // If invalid register, ignore the request
       reg_position = 0;
       return;
    }
    
    // If any bytes left, write them to registers
    while (howMany) {
       byte value = TinyWireS.receive();
       howMany--;
      switch (reg_position) {
      case 0:
         if (value >= 17 && value <= 28)
            OCR1A = value;
         break;
      case 1:
         if (value >= 17 && value <= 28)
            OCR1B = value;
         break;
      default:
         break;
      }
      reg_position++;
      if (reg_position >= reg_count)
         reg_position = 0;
    }
}


void setup()
{
    // Servos like PWM durations around 1.5 ms with periods of 15-20 ms
    //
    // Set Timer/Counter 1 to increment at 15625 Hz (8 MHz/512) and
    // count to 255. This gives a PWM period of ~16 ms (256/15625)
    // See ATtiny25/45/85 datasheet
    // OCR1C = value to count up to (period for PWM)
    // COM1A1,COM1A0 = 1,0  # set output low when counter matches OCR1A value (See Table 13-1)
    OCR1C = 255; // Count to 255, then reset counter
    OCR1A = 22; // servo idle (~1.5 ms)
    OCR1B = 22; // servo idle (~1.5 ms)
    TCCR1 = _BV(PWM1A) | _BV(COM1A1) | 0xa; // Enable PWM1A; CK/512 (Table 12-5) NOTE: Table 12-5 not for PWM Mode???
    GTCCR = _BV(PWM1B) | _BV(COM1B1);       // Enable PWM1B

    pinMode(1, OUTPUT); // PWM1A
    pinMode(4, OUTPUT); // PWM1B
    pinMode(3, INPUT);  // Battery level

    /**
     * Reminder: taking care of pull-ups is the masters job
     */

    TinyWireS.begin(I2C_SLAVE_ADDRESS);
    TinyWireS.onReceive(receiveEvent);
    TinyWireS.onRequest(requestEvent);
}

void loop()
{
    /**
     * This is the only way we can detect stop condition (http://www.avrfreaks.net/index.php?name=PNphpBB2&file=viewtopic&p=984716&sid=82e9dc7299a8243b86cf7969dd41b5b5#984716)
     * it needs to be called in a very tight loop in order not to miss any (REMINDER: Do *not* use delay() anywhere, use tws_delay() instead).
     * It will call the function registered via TinyWireS.onReceive(); if there is data in the buffer on stop.
     */
    TinyWireS_stop_check();
}

