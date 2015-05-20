/* LiquidCrystalSerial
 *
 * This is a base class for control of a HD44780-based LCD display.
 * It should be subclassed to provide specific implementation of the
 * communication routines for specific hardware.
 *
 * For example, the standard OEM MBI hardware uses a shift register to
 * send data to the LCD display.  Other hardware might use I2C to do
 * accomplish the same thing.
 *
 * This base class contains the initialization and convenience methods
 * that are similar for all LCD displays.  These methods rely on the
 * subclass' implementaiton of the low level communication routines
 * such as send, writeSerial, write4bits, and pulseEnable.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include "Configuration.hh"
#include "LiquidCrystalSerial.hh"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <util/delay.h>
#include "TWI.hh"

// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set:
//    DL = 1; 8-bit interface data
//    N = 0; 1-line display
//    F = 0; 5x8 dot character font
// 3. Display on/off control:
//    D = 0; Display off
//    C = 0; Cursor off
//    B = 0; Blinking off
// 4. Entry mode set:
//    I/D = 1; Increment by 1
//    S = 0; No shift
//
// Note, however, that resetting the Arduino doesn't reset the LCD, so we
// can't assume that its in that state when a sketch starts (and the
// LiquidCrystal constructor is called).

// Nothing to construct
LiquidCrystalSerial::LiquidCrystalSerial() {}

// Initialization of a standard HD44780 display
void LiquidCrystalSerial::begin(uint8_t cols, uint8_t lines, uint8_t dotsize) {

  if (lines > 1) {
    _displayfunction |= LCD_2LINE;
  }
  _numlines = lines;
  _numCols = cols;

  // for some 1 line displays you can select a 10 pixel high font
  if ((dotsize != LCD_5x8DOTS) && (lines == 1)) {
    _displayfunction |= LCD_5x10DOTS;
  }

  // SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
  // according to datasheet, we need at least 40ms after power rises above 2.7V
  // before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
  _delay_us(50000);

  // Now we pull both RS and R/W low to begin commands
  writeSerial(0b00000000);

  // this is according to the hitachi HD44780 datasheet
  // figure 24, pg 46

  // we start in 8bit mode, try to set 4 bit mode
  write4bits(0x03, false);
  _delay_us(4500); // wait min 4.1ms

  // second try
  write4bits(0x03, false);
  _delay_us(4500); // wait min 4.1ms

  // third go!
  write4bits(0x03, false);
  _delay_us(150);

  // finally, set to 8-bit interface
  write4bits(0x02, false);

  // finally, set # lines, font size, etc.
  command(LCD_FUNCTIONSET | _displayfunction);

  // turn the display on with no cursor or blinking default
  _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;

#if defined(HAS_VIKI_INTERFACE)
  _displaycontrol |= LCD_BACKLIGHT;
#endif

  display();

  // clear it off
  clear();

  // Initialize to default text direction (for romance languages)
  _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
  // set the entry mode
  command(LCD_ENTRYMODESET | _displaymode);

  // program special characters
  uint8_t down[8] = { 0x00,   // 0000000
                      0x00,   // 0000000
                      0x00,   // 0000000
                      0x00,   // 0000000
                      0x22,   // 0100010
                      0x14,   // 0010100
                      0x08,   // 0001000
                      0x00 }; // 0000000

  uint8_t folder_in[8] = { 0x08, // 01000
                           0x0C, // 01100
                           0x0E, // 01110
                           0x0F, // 01111
                           0x0E, // 01110
                           0x0C, // 01100
                           0x08, // 01000
                           0x00  // 00000
  };

  uint8_t folder_out[8] = { 0x04, // 00100
                            0x0C, // 01100
                            0x1F, // 11111
                            0x0D, // 01101
                            0x05, // 00101
                            0x01, // 00001
                            0x1E, // 11110
                            0x00  // 00000
  };

#if 0
    //Custom extruder / platform heating and arrow
    //characters (Courtesy of Erwin Ried)

    uint8_t extruder_normal[8] = {
	    0x11,	//10001
	    0x1F,	//11111
	    0x0A,	//01010
	    0x0A,	//01010
	    0x0A,	//01010
	    0x0E,	//01110
	    0x04,	//00100
	    0x00};	//00000

    uint8_t extruder_heating[8] = {
	    0x11,	//10001
	    0x1F,	//11111
	    0x0E,	//01110
	    0x0E,	//01110
	    0x0E,	//01110
	    0x0E,	//01110
	    0x04,	//00100
	    0x00};	//00000

    uint8_t platform_normal[8] = {
	    0x12,	//10010
	    0x09,	//01001
	    0x12,	//10010
	    0x09,	//01001
	    0x00,	//00000
	    0x1F,	//11111
	    0x11,	//10001
	    0x00};	//00000

    uint8_t platform_heating[8] = {
	    0x12,	//10010
	    0x09,	//01001
	    0x12,	//10010
	    0x09,	//01001
	    0x00,	//00000
	    0x1F,	//11111
	    0x1F,	//11111
	    0x00};	//00000
#endif

  createChar(LCD_CUSTOM_CHAR_DOWN, down);
  createChar(LCD_CUSTOM_CHAR_FOLDER, folder_in);
  createChar(LCD_CUSTOM_CHAR_RETURN, folder_out);

#if 0
    createChar(LCD_CUSTOM_CHAR_EXTRUDER_NORMAL, extruder_normal);
    createChar(LCD_CUSTOM_CHAR_EXTRUDER_HEATING, extruder_heating);
#endif
}

/********** high level commands, for the user! */
void LiquidCrystalSerial::clear() {
  command(LCD_CLEARDISPLAY); // clear display, set cursor position to zero
  _delay_us(2000);           // this command takes a long time!
}

void LiquidCrystalSerial::home() {
  command(LCD_RETURNHOME); // set cursor position to zero
  _delay_us(2000);         // this command takes a long time!
}

// A faster version of home()
void LiquidCrystalSerial::homeCursor() { setCursor(0, 0); }

void LiquidCrystalSerial::setRow(uint8_t row) { setCursor(0, row); }

// A faster version of clear and fast home() combined
// Since this is a common combination of calls, it saves code
// space to combine them into one.
void LiquidCrystalSerial::clearHomeCursor() {
  clear();
  setCursor(0, 0);
}

void LiquidCrystalSerial::setCursor(uint8_t col, uint8_t row) {
  int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
  if (row > _numlines) {
    row = _numlines - 1; // we count rows starting w/0
  }

  _xcursor = col; _ycursor = row;
  command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// If col or row = -1, then the current position is retained
// useful for controlling x when y is already positions, especially
// within drawItem
void LiquidCrystalSerial::setCursorExt(int8_t col, int8_t row) {
  setCursor((col == -1) ? _xcursor : col, (row == -1) ? _ycursor : row);
}

// Turn the display on/off (quickly)
void LiquidCrystalSerial::noDisplay() {
  _displaycontrol &= ~LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystalSerial::display() {
  _displaycontrol |= LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turns the underline cursor on/off
void LiquidCrystalSerial::noCursor() {
  _displaycontrol &= ~LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystalSerial::cursor() {
  _displaycontrol |= LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turn on and off the blinking cursor
void LiquidCrystalSerial::noBlink() {
  _displaycontrol &= ~LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystalSerial::blink() {
  _displaycontrol |= LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// These commands scroll the display without changing the RAM
void LiquidCrystalSerial::scrollDisplayLeft(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void LiquidCrystalSerial::scrollDisplayRight(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void LiquidCrystalSerial::leftToRight(void) {
  _displaymode |= LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This is for text that flows Right to Left
void LiquidCrystalSerial::rightToLeft(void) {
  _displaymode &= ~LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'right justify' text from the cursor
void LiquidCrystalSerial::autoscroll(void) {
  _displaymode |= LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'left justify' text from the cursor
void LiquidCrystalSerial::noAutoscroll(void) {
  _displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void LiquidCrystalSerial::createChar(uint8_t location, uint8_t charmap[]) {
  location &= 0x7; // we only have 8 locations 0-7
  uint8_t cmd = LCD_SETCGRAMADDR | (location << 3);

  // write each character twice as sometimes there are signal issues
  for(uint8_t j = 2; j; j--)
  {
    command(cmd);
    uint8_t *map = charmap;
    for (int i = 8; i; i--) {
      write(*map++);
    }
  }
}

/*********** mid level commands, for sending data/cmds */

void LiquidCrystalSerial::command(uint8_t value) { send(value, false); }

inline void LiquidCrystalSerial::write(uint8_t value) {
  send(value, true);
  _xcursor++;
  if (_xcursor >= _numCols)
    setCursor(0, _ycursor + 1);
}

void LiquidCrystalSerial::writeInt(uint16_t value, uint8_t digits) {

    if(digits > 5)
        digits = 5;
    writeInt32(value, digits);
}

void LiquidCrystalSerial::moveWriteInt(uint8_t col, uint8_t row, uint16_t value, uint8_t digits) {
     setCursor(col, row);
     writeInt(value, digits);
}

void LiquidCrystalSerial::writeInt32(uint32_t value, uint8_t digits) {

     uint32_t currentDigit = 1;
     uint32_t nextDigit;
     bool nonzero_seen = false;

     if ( digits > 9 )
	  digits = 9;

     for (uint8_t i = digits; i; i--)
	  currentDigit *= 10;

     for (uint8_t i = digits; i; i--) {
	  nextDigit = currentDigit / 10;
	  char c;
	  int8_t d = (value % currentDigit) / nextDigit;
	  if ( nonzero_seen || d != 0 || i == 1) {
	       c = d + '0';
	       nonzero_seen = true;
	  }
	  else
	       c = ' ';
	  write(c);
	  currentDigit = nextDigit;
     }
}

//From: http://www.arduino.cc/playground/Code/PrintFloats
//tim [at] growdown [dot] com   Ammended to write a float to lcd
//If rightJusityToCol = 0, the number is left justified, i.e. printed from the
//current cursor position.  If it's non-zero, it's right justified to end at rightJustifyToCol column.

#define MAX_FLOAT_STR_LEN 20

void LiquidCrystalSerial::writeFloat(float value, uint8_t decimalPlaces, uint8_t rightJustifyToCol) {
        // this is used to cast digits
        int digit;
        float tens = 0.1;
        int tenscount = 0;
        int i;
        float tempfloat = value;
	uint8_t p = 0;
	char str[MAX_FLOAT_STR_LEN + 1];

        // make sure we round properly. this could use pow from <math.h>, but doesn't seem worth the import
        // if this rounding step isn't here, the value  54.321 prints as 54.3209

        // calculate rounding term d:   0.5/pow(10,decimalPlaces)
        float d = 0.5;
        if (value < 0) d *= -1.0;

        // divide by ten for each decimal place
        for (i = decimalPlaces; i; i--) d/= 10.0;

        // this small addition, combined with truncation will round our values properly
        tempfloat +=  d;

        // first get value tens to be the large power of ten less than value
        // tenscount isn't necessary but it would be useful if you wanted to know after this how many chars the number will take

        tempfloat = fabsf(tempfloat);
        while ((tens * 10.0) <= tempfloat) {
                tens *= 10.0;
                tenscount += 1;
        }

        // write out the negative if needed
        if (value < 0) str[p++] = '-';

        if (tenscount == 0) str[p++] = '0';

        for (i = tenscount; i; i--) {
                digit = (int) (tempfloat/tens);
                str[p++] = digit + '0';
                tempfloat = tempfloat - ((float)digit * tens);
                tens /= 10.0;
        }

        // if no decimalPlaces after decimal, stop now and return
        if (decimalPlaces > 0) {
		// otherwise, write the point and continue on
		str[p++] = '.';

		// now write out each decimal place by shifting digits one by one into the ones place and writing the truncated value
		for (i = decimalPlaces; i; i--) {
			tempfloat *= 10.0;
			digit = (int) tempfloat;
			str[p++] = digit+'0';
			// once written, subtract off that digit
			tempfloat = tempfloat - (float) digit;
		}
	}

	str[p] = '\0';

	if ( rightJustifyToCol ) {
		setCursorExt(rightJustifyToCol - p, -1);
	}
	writeString(str);
}

char* LiquidCrystalSerial::writeLine(char* message) {
	char* letter = message;
	while (*letter != 0 && *letter != '\n') {
		//INTERFACE_RLED.setValue(true);
		write(*letter);
		letter++;

	}
	return letter;
}

void LiquidCrystalSerial::writeString(char message[]) {
	char* letter = message;
	while (*letter != 0) {
		write(*letter);
		letter++;
	}
}

void LiquidCrystalSerial::writeFromPgmspace(const prog_uchar message[]) {
	char letter;
	while ((letter = pgm_read_byte(message++)))
		write(letter);
}

void LiquidCrystalSerial::moveWriteFromPgmspace(uint8_t col, uint8_t row,
						const prog_uchar message[]) {
	setCursor(col, row);
	writeFromPgmspace(message);
}
