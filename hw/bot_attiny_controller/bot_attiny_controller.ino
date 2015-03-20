/**
 * PawelBot servo controller for the Raspberry Pi.
 *
 * This program runs on an ATtiny85 connected to the RPi via the
 * I2C lines. It generates the PWM signals needed to control two
 * continuous turn servos. It also has an ADC input for checking
 * the battery level.
 *
 * To test on Raspian:
 *
 * Connect the ATtiny85's I2C pins to the I2C pins on the
 * Raspberry Pi's GPIO connector
 *
 * # sudo apt-get install i2c-tools
 * # sudo raspi-config
 * In raspi-config, enable I2C through the advanced menu.
 *
 * # sudo modprobe i2c-dev
 * # sudo i2cdetect -y 1
 *      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
 * 00:          -- 04 -- -- -- -- -- -- -- -- -- -- --
 * 10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 * 70: -- -- -- -- -- -- -- --
 *
 * If the device doesn't show up (i.e. the 04), then
 * check the connections.
 *
 * # sudo i2cget -y 1 4 0 w    ## get left servo pulse duration
 * 0x05dc                      ## 0x05dc = 1500 microsecond pulse
 *
 * # sudo i2cget -y 1 4 2 w    ## get right servopulse duration
 * 0x05dc
 *
 * # sudo i2cget -y 1 4 4 w    ## get battery voltage
 * 0x138e                      ## 0x138e = 5006 microVolts
 *
 * # sudo i2cset -y 1 4 0 1200 ## set the left servo pulse to 1200 us
 * # sudo i2cset -y 1 4 2 1500 ## set the right right pulse to 1500 us
 *
 * *** There are intermittent I2C failures. I have not tracked them down. ***
 *
 * Programming an ATtiny85 with an Arduino Uno
 *
 * 1. Untar tiny.tgz into your ~/sketchbook/hardware or whereever your
 *    Arduino sketchbook directory is. These files come from http://hlt.media.mit.edu/?p=1229,
 *    but they're not always available. As a bonus, I've already made
 *    a boards.txt that defines the right ATtiny85 configuration for this project.
 * 2. In ~/sketchbook/libraries, run:
 *       git clone https://github.com/rambo/TinyWire.git
 * 3. Restart the Arduino IDE if you have it open, so that it finds the TinyWireS library
 * 4. In the Arduino IDE, program the ArduinoISP firmware onto the Uno
 *    (it's an example Arduino program)
 * 5. In Arduino IDE, set the board to "ATtiny85 @ 8 MHz (internal oscillator; BOD disabled)
 * 6. Set the programmer to "Arduino as ISP"
 *
 *  -- Setup steps are done --
 *
 * 7. Wire the Uno to the ATtiny85 like this:
 *      Arduino +5V    ---> ATtiny Pin 8
 *      Arduino Ground ---> ATtiny Pin 4
 *      Arduino Pin 10 ---> ATtiny Pin 1
 *      Arduino Pin 11 ---> ATtiny Pin 5
 *      Arduino Pin 12 ---> ATtiny Pin 6
 *      Arduino Pin 13 ---> ATtiny Pin 7
 *      Arduino Reset, Arduino group -> connect with 10 uF capacitor
 * 8. Burn bootloader (VERY, VERY IMPORTANT. I learned the hard way.)
 * 9. Compile and upload this program.
 */


/**
 * ATtiny85 pin assignments
 *
 * Pin   Function
 * 1     NRESET
 * 2     PB3 - Battery level/2 * 3.3/255
 * 3     PB4 - Right servo (OC1B)
 * 4     GND
 * 5     PB0 - I2C SDA
 * 6     PB1 - Left servo (OC1A)
 * 7     PB2 - I2C SCL
 * 8     VCC (+3.3V)
 */

#define I2C_SLAVE_ADDRESS 0x4
#include <TinyWireS.h>

enum I2cRegisters {
    I2cRegLeftServoLo = 0,
    I2cRegLeftServoHi,
    I2cRegRightServoLo,
    I2cRegRightServoHi,
    I2cRegBatteryLo,
    I2cRegBatteryHi,
    I2cRegReadRequestCount,
    I2cRegWriteRequestCount,
    I2cRegCount
};

static volatile uint8_t currentRegister = 0;

static uint16_t leftServoMicros = 1500;
static uint16_t rightServoMicros = 1500;
static uint16_t batteryMicroVolts = 0;
static uint8_t readRequestCount = 0;
static uint8_t writeRequestCount = 0;

#if F_CPU == 8000000L
// Divide FCLK by 64 -> 8 MHz/64 = 125 KHz (each tick is 8 uS)
#define TCCR1_CS_BITS 0x7
#else
#error Unexpected CPU speed
#endif

/*
 * Handle a 'get' request from the I2C master.
 */
void requestEvent()
{
    readRequestCount++;

    byte value;
    switch (currentRegister) {
    case I2cRegLeftServoLo:
        value = lowByte(leftServoMicros);
        break;
    case I2cRegLeftServoHi:
        value = highByte(leftServoMicros);
        break;
    case I2cRegRightServoLo:
        value = lowByte(rightServoMicros);
        break;
    case I2cRegRightServoHi:
        value = highByte(rightServoMicros);
        break;
    case I2cRegBatteryLo:
        value = lowByte(batteryMicroVolts);
        break;
    case I2cRegBatteryHi:
        value = highByte(batteryMicroVolts);
        break;
    case I2cRegReadRequestCount:
        value = readRequestCount;
        break;
    case I2cRegWriteRequestCount:
        value = writeRequestCount;
        break;
    default:
        value = 0;
        break;
    }
    TinyWireS.send(value);
    currentRegister++;
    if (currentRegister >= I2cRegCount)
        currentRegister = 0;
}

/*
 * Remove everything in the receive buffer.
 * This is usually called in case of error
 * where we're not quite sure what the user
 * was doing or whether there was an error.
 */
static void flushReceiveBuffer()
{
    while (TinyWireS.available())
        TinyWireS.receive();
}

/*
 * Handle a 'set' request from the I2C master.
 */
void receiveEvent(uint8_t howMany)
{
    static uint16_t nextLeftServoMicros = 0;
    static uint16_t nextRightServoMicros = 0;

    // Sanity check
    if (howMany < 1 || howMany > I2cRegCount) {
        flushReceiveBuffer();
        return;
    }

    // Read register position
    currentRegister = TinyWireS.receive();
    howMany--;
    writeRequestCount++;

    if (currentRegister >= I2cRegCount) {
        // If invalid register, ignore the request
        currentRegister = 0;
        flushReceiveBuffer();

        // Idle the servos since we're confused
        // This is mostly for debug.
        leftServoMicros = 1500;
        rightServoMicros = 1500;
        return;
    }

    // If any bytes left, write them to registers
    while (howMany) {
        byte value = TinyWireS.receive();
        howMany--;
        switch (currentRegister) {
        case I2cRegLeftServoLo:
            nextLeftServoMicros = (nextLeftServoMicros & 0xff00) | value;
            break;
        case I2cRegLeftServoHi:
            nextLeftServoMicros = (nextLeftServoMicros & 0x00ff) | (value << 8);
            break;
        case I2cRegRightServoLo:
            nextRightServoMicros = (nextRightServoMicros & 0xff00) | value;
            break;
        case I2cRegRightServoHi:
            nextRightServoMicros = (nextRightServoMicros & 0x00ff) | (value << 8);
            break;
        default:
            break;
        }
        currentRegister++;
        if (currentRegister >= I2cRegCount)
            currentRegister = 0;
    }
    writeRequestCount += howMany;

    if (nextLeftServoMicros >= 1000 && nextLeftServoMicros <= 2000)
        leftServoMicros = nextLeftServoMicros;

    if (nextRightServoMicros >= 1000 && nextRightServoMicros <= 2000)
        rightServoMicros = nextRightServoMicros;
}

void setup()
{
    // Servos like PWM durations around 1.5 ms with periods of 15-20 ms
    // The important part is the duration.

    // Since the ATtiny85 only has an 8 bit PWM, there's not enough precision
    // to just use a PWM (aka Timer/Counter). To see this, we'd like to get
    // 180 or more steps between 1-2 ms (so that we can do 0-180 degrees with
    // 1 degree resolution). This requires about 5 uS resolution. To get to
    // 20 ms would require counting to 4000, and that's not considering that
    // the ATtiny85 can't be set to count at any frequency.

    // What we do instead is send a pulse using Timer/Counter1 and then busy
    // wait between pulses. The busy wait doesn't need to be perfect, so if
    // I2C handling delays things a little, that's ok.

    pinMode(1, OUTPUT); // Left servo
    pinMode(4, OUTPUT); // Right servo
    pinMode(3, INPUT);  // Battery level

    /**
     * Reminder: taking care of pull-ups is the masters job
     */
    TinyWireS.begin(I2C_SLAVE_ADDRESS);
    TinyWireS.onReceive(receiveEvent);
    TinyWireS.onRequest(requestEvent);
}

uint8_t mapServoMicros(uint16_t micros)
{
    return (uint8_t) (uint32_t(micros - 16) * 254 / 2032) + 1;
}

void loop()
{
    // Our target is around 20 ms between pulses, so wait for 19 ms
    // between pulses to give time for ADC read and calculations.
    tws_delay(19); // Can't call delay() due to TinyWireS limitation

    // Sample the battery voltage (this takes about 100 microseconds according to docs)
    // The battery voltage is halved by a resister divider so that 6 V is
    // less that the 3.3 V reference voltage.
    batteryMicroVolts = (uint16_t) (uint32_t(analogRead(3)) * 2 * 3300 / 1023); // 6600 microvolts at 1023 counts

    // Start the counter over from 0 (Have to do this first to avoid spurious pulse since
    // there's no stopping the timer/counter)
    TCNT1 = 0;

    // Calculate the new pulse durations
    // This calculation takes more than a few microseconds, so will need to restart the
    // TCNT1 to accurately measure the duration
    OCR1A = mapServoMicros(leftServoMicros);
    OCR1B = mapServoMicros(rightServoMicros);

    // Force both outputs high.
    // The way this is done is by telling the Timer/Counter to set the outputs
    // high on match and then artificially trigger a match.
    // NOTE: You can't just set the output high since the Timer/Counter is now
    //       controlling it.
    TCCR1 = (1 << COM1A1) | (1 << COM1A0) | TCCR1_CS_BITS;
    GTCCR = (1 << COM1B1) | (1 << COM1B0) | (1 << FOC1B) | (1 << FOC1A);

    // Start the counter counting for real now that the outputs are officially high
    TCNT1 = 0;

    // Tell the Timer/Counter to set the outputs low on match (a real match this time)
    GTCCR = (1 << COM1B1) | (0 << COM1B0);
    TCCR1 = (1 << COM1A1) | (0 << COM1A0) | TCCR1_CS_BITS;
}

