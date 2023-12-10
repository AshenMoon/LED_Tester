# LED_Tester
Tests LEDs in 1-20 mA range.

Calculates the series resistor depending on the connected voltage.

Based on the 1602 LCD display version by mircemk:

https://hackaday.io/project/193787-how-to-make-arduino-led-tester-resistor-calc

The new version allows to use either 1602 LCD or SSD1306 0.96-inch I2C display.

In order to use 1602 LCD you need to comment the #define SSD1306_DISPLAY string
