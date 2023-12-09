/*
Arduino LED Tester
DPIN--39R--+--10R---TESTLED---GND
           |      |         |
          470u    ATOP     ABOT
           |
          GND

 Measures LED characteristics by charging up the cap to deliver target current and find forward voltage
 From target current, we can calculate R to be used with a design supply voltage and a matching part number.
  */

#define SSD1306_DISPLAY // Version for the SSD1306 0.96-inch display I2C 

#include <Wire.h>
#ifdef SSD1306_DISPLAY
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#else
#include <LiquidCrystal_I2C.h>
#endif

//lcd update interval
#define LCDINT 500
//key check interval
#define KEYINT 200
//pin for buttons
#define KEYPIN A0
//button constants
#define btnRIGHT 6
#define btnUP 5
#define btnDOWN 4
#define btnLEFT 3
#define btnSELECT 2
#define btnNONE (-1)

#ifdef SSD1306_DISPLAY
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#define OLED_RESET 4  //OLED reset on pin 4
#else
LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD adress
#endif

//Globals for display
long itest = 10;            //test current, in mA
long vset = 14000;          //display voltage in mV
long vled, vrr, irr, pset;  //LED voltage, Resistor voltage, resistor current, display power
//resistors in Jaycar 1/2 W range, part nos start at RR0524 for 10R
#define RCOUNT 121
const PROGMEM long rvals[] = { 10, 11, 12, 13, 15, 16, 18, 20, 22, 24, 27, 30, 33, 36, 39, 43, 47, 51, 56, 62, 68, 75, 82, 91, 100, 110, 120, 130, 150, 160, 180, 200, 220, 240, 270, 300, 330, 360, 390, 430, 470, 510, 560, 620, 680, 750, 820, 910, 1000, 1100, 1200, 1300, 1500, 1600, 1800, 2000, 2200, 2400, 2700, 3000, 3300, 3600, 3900, 4300, 4700, 5100, 5600, 6200, 6800, 7500, 8200, 9100, 10000, 11000, 12000, 13000, 15000, 16000, 18000, 20000, 22000, 24000, 27000, 30000, 33000, 36000, 39000, 43000, 47000, 51000, 56000, 62000, 68000, 75000, 82000, 91000, 100000, 110000, 120000, 130000, 150000, 160000, 180000, 200000, 220000, 240000, 270000, 300000, 330000, 360000, 390000, 430000, 470000, 510000, 560000, 620000, 680000, 750000, 820000, 910000, 1000000 };
long lastlcd = 0;  //time of last lcd update
long lastkey = 0;  //time of last key check
int lcdflash = 0;  //lcd flashing phase variable
long pdes = 0;
long rval = 0;       //calculated resistor value for display
long rindex = 0;     //index of selected resistor in rvals[]
int pwmout = 0;  //pwm output of current driver
int rvalid = 0;  //flag if resistor value is valid
//pins for test interface
#define ATOP A2
#define ABOT A3
#define DPIN 3
#define OSAMP 16

void setup() {
  Serial.begin(9600);
#ifdef SSD1306_DISPLAY
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  //delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);

//----------------------------------
/*  display.clearDisplay();
  display.setTextSize(2);        //set text sizez
  display.setTextColor(WHITE);   //set text color
  display.setCursor(0, 0);       //set cursor
  //milliamps
  display.print("Init OK");
  display.display();*/
#else
  lcd.begin(16, 2);  //lcd
  lcd.clear();
  lcd.backlight();
#endif
  pinMode(DPIN, OUTPUT);  //pwm pin
//  for (long i = 0; i < RCOUNT; i++) {
//    long a = pgm_read_dword_near(rvals + i);
//    Serial.println(a);
//  }
}


void loop() {
  long atop, abot, arr;  //analog sample values
  rvalid = 0;            //set flag to not valid
  atop = analogoversample(ATOP, OSAMP) / OSAMP;
  abot = analogoversample(ABOT, OSAMP) / OSAMP;
  arr = atop - abot;  //this is the analog value across the 10R sense resistor
  if (arr < 0) {      //sanity check
    arr = 0;
  }
  vled = abot * 5000 / 1023;  //5000mV=1023 => voltage across LED
  vrr = arr * 5000 / 1023;    //voltage across sense resistor
  irr = vrr / 10;             //led and resistor current in mA (cos it's a 10R resistor)

  if (irr < itest) {
    ++pwmout;
    if (pwmout > 255) {
      pwmout = 255;
    }
  }  //ramp up current if too low
  if (irr > itest) {
    --pwmout;
    if (pwmout < 0) {
      pwmout = 0;
    }
  }  //ramp down if too high
  if (irr > 24) {
    pwmout -= 5;
    if (pwmout < 0) {
      pwmout = 0;
    }
  }  //ramp down quick if too too high
  if (irr > itest * 3) {
    pwmout -= 5;
    if (pwmout < 0) {
      pwmout = 0;
    }
  }                                   //ramp down quick if too too high
  analogWrite(DPIN, pwmout);          //output new PWM
  rval = (vset - vled) / itest;       //mV/mV => ohms resistance of display resistor
  //Serial.println(rval);
  //Serial.println(" ");
  for (int i = 0; i < RCOUNT; i++) {  //find next highest E24 value
    long currRval = pgm_read_dword_near(rvals + i);
    if (currRval >= rval) {
      rindex = i;
      rval = currRval;
      //Serial.println(rval);
      //Serial.println(" ");
      i = RCOUNT + 1;
      rvalid = 1;
    }
  }
  if (abs(irr - itest) > (itest / 5) + 1) {  //has current settled within 20%?
    //Serial.println(irr);
    rvalid = 0;
  }
  if (vled > vset) {  //if vled>vset, no valid resistor exists
    //Serial.println(vled);
    rvalid = 0;
  }
  pset = 0;      //work out dissipation in ballast resistor if valid)
  if (rvalid) {  //this will be in microwatts (milliamps squared)
    pset = itest * itest * rval;
  }

  if (millis() - lastlcd > LCDINT) {  //check if display due to be updated
    lastlcd = millis();
    dolcd();                  //update display
    lcdflash = 1 - lcdflash;  //toggle flash variable
  }
  if (millis() - lastkey > KEYINT) {  //check if keys due to be checked
    lastkey = millis();
    dobuttons();
  }
  delay(1);
}  //end of loop

#ifdef SSD1306_DISPLAY
void dolcd() {
  display.clearDisplay();
  display.setTextSize(2);        //set text sizez
  display.setTextColor(WHITE);   //set text color
  display.setCursor(0, 0);       //set cursor
  //milliamps
  display.print("It=");
  if (lcdflash || rvalid) {  //show if correct if on flashing phase
    if (itest > 9) {
      //Serial.println((itest / 10) % 10);
      //display.print(((itest / 10) % 10) + '0');
      display.print(((itest / 10) % 10));
      //display.print('0');
    } else {
      display.print(' ');
    }  //blank tens if zero
    //Serial.println(itest % 10);
    //display.print((itest % 10) + '0');
    display.print((itest % 10));
    //display.print('0');
  } else {  //blank if not
    display.print(' ');
    display.print(' ');
  }
//  Serial.println(lcdflash);
//  Serial.println(rvalid);
//  Serial.println(itest);
//  Serial.println(" ");
  display.print('m');
  display.print('A');
  display.println(' ');

  //VLED
  //display.print(((vled / 1000) % 10) + '0');
  display.print(((vled / 1000) % 10));
  //display.print('0');
  display.print('.');
  //display.print(((vled / 100) % 10) + '0');
  display.print(((vled / 100) % 10));
  //display.print('0');
  display.print('V');
  display.print(' ');

  display.display();
}
#else
void dolcd() {
  lcd.setCursor(0, 0);  //first line
  //milliamps
  if (lcdflash || rvalid) {  //show if correct if on flashing phase
    if (itest > 9) {
      lcd.write(((itest / 10) % 10) + '0');
    } else {
      lcd.write(' ');
    }  //blank tens if zero
    lcd.write((itest % 10) + '0');
  } else {  //blank if not
    lcd.write(' ');
    lcd.write(' ');
  }
  lcd.write('m');
  lcd.write('A');
  lcd.write(' ');
  //VLED
  lcd.write(((vled / 1000) % 10) + '0');
  lcd.write('.');
  lcd.write(((vled / 100) % 10) + '0');
  lcd.write('V');
  lcd.write(' ');

  //actual LED current
  if (irr > 9) {
    lcd.write(((irr / 10) % 10) + '0');
  } else {
    lcd.write(' ');
  }  //blank tens if zero
  lcd.write((irr % 10) + '0');
  lcd.write('m');
  lcd.write('A');
  lcd.write(' ');
  if ((pset > 499999) && (lcdflash)) {
    lcd.write('P');
  } else {
    lcd.write(' ');
  }  //flash P if power above 1/2 watt

  lcd.setCursor(0, 1);  //second line
  //
  if (vset > 9999) {
    lcd.write(((vset / 10000) % 10) + '0');
  } else {
    lcd.write(' ');
  }                                       //tens of vset, blank if zero
  lcd.write(((vset / 1000) % 10) + '0');  //units of vset
  lcd.write('V');
  lcd.write(' ');

  if (rvalid) {
    lcdprintrval(rval);  //resistor value (4 characters)
    lcd.write(' ');
    lcdprintpartno(rindex);  //resistor part no (6 characters)
    if (pset > 499999) {
      lcd.write('!');
    } else {
      lcd.write(' ');
    }  //show ! if power above 1/2 watt
  } else {
    lcd.write(' ');
    lcd.write('-');
    lcd.write('-');
    lcd.write('-');
    lcd.write(' ');
    lcd.write('-');
    lcd.write('-');
    lcd.write('-');
    lcd.write('-');
    lcd.write('-');
    lcd.write('-');
    lcd.write(' ');
  }
}
#endif

#ifdef SSD1306_DISPLAY
void lcdprintpartno(int index) {
  // Don't print part number on SSD1306 display
}
#else
void lcdprintpartno(int index) {
  //part number
  lcd.write('R');
  lcd.write('R');
  lcd.write('0');
  lcd.write((((index + 524) / 100) % 10) + '0');  //part no's start at RR0524 for 10R
  lcd.write((((index + 524) / 10) % 10) + '0');
  lcd.write((((index + 524)) % 10) + '0');
}
#endif

#ifdef SSD1306_DISPLAY
void lcdprintrval(long rval) {

}
#else
void lcdprintrval(long rval) {  //print a value in 10k0 format, always outputs 4 characters
  long mult = 1;
  long modval;
  if (rval > 999) { mult = 1000; }
  if (rval > 999999) { mult = 1000000; }
  modval = (10 * rval) / mult;  //convert to final format, save a decimal place
  if (modval > 999) {           //nnnM
    lcd.write(((modval / 1000) % 10) + '0');
    lcd.write(((modval / 100) % 10) + '0');
    lcd.write(((modval / 10) % 10) + '0');
    lcdprintmult(mult);
  } else {
    if (modval > 99) {  //nnMn
      lcd.write(((modval / 100) % 10) + '0');
      lcd.write(((modval / 10) % 10) + '0');
      lcdprintmult(mult);
      lcd.write(((modval) % 10) + '0');
    } else {  //_nMn
      lcd.write(' ');
      lcd.write(((modval / 10) % 10) + '0');
      lcdprintmult(mult);
      lcd.write(((modval) % 10) + '0');
    }
  }
}
#endif

#ifdef SSD1306_DISPLAY
void lcdprintmult(long mult) {

}
#else
void lcdprintmult(long mult) {  //helper function to print multiplier
  switch (mult) {
    case 1:
      lcd.print('R');
      break;

    case 1000:
      lcd.print('k');
      break;

    case 1000000:
      lcd.print('M');
      break;

    default:
      lcd.print('?');
      break;
  }
}
#endif

int read_LCD_buttons() {
  int adc_key_in = 0;
  adc_key_in = analogRead(KEYPIN);            // read the value from the sensor
  delay(5);                                   //switch debounce delay. Increase this delay if incorrect switch selections are returned.
  int k = (analogRead(KEYPIN) - adc_key_in);  //gives the button a slight range to allow for a little contact resistance noise
  if (5 < abs(k))
    return btnNONE;  // double checks the keypress. If the two readings are not equal +/-k value after debounce delay, it tries again.
  // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
  // we add approx 50 to those values and check to see if we are close
  if (adc_key_in > 1000)
    return btnNONE;  // We make this the 1st option for speed reasons since it will be the most likely result
  if (adc_key_in < 50)
    return btnRIGHT;
  if (adc_key_in < 195)
    return btnUP;
  if (adc_key_in < 380)
    return btnDOWN;
  if (adc_key_in < 555)
    return btnLEFT;
  if (adc_key_in < 790)
    return btnSELECT;
  return btnNONE;  // when all others fail, return this...
}

void dobuttons() {  //updates variables. debounces by only sampling at intervals
  int key;
  key = read_LCD_buttons();
  if (key == btnLEFT) {
    itest = itest - 1;
    if (itest < 1) {
      itest = 1;
    }
  }
  if (key == btnRIGHT) {
    itest = itest + 1;
    if (itest > 20) {
      itest = 20;
    }
  }
  if (key == btnUP) {
    vset = vset + 1000;
    if (vset > 99000) {
      vset = 99000;
    }
  }
  if (key == btnDOWN) {
    vset = vset - 1000;
    if (vset < 0) {
      vset = 0;
    }
  }
}

long analogoversample(int pin, int samples) {  //read pin samples times and return sum
  long n = 0;
  for (int i = 0; i < samples; i++) {
    n = n + analogRead(pin);
  }
  return n;
}
