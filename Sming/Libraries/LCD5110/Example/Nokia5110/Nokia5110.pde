#include <LCD5110.h>void setup(void){  LcdInitialise();  LcdClear();  drawBox();   gotoXY(7,1);  LcdString("Nokia 5110");  gotoXY(4,2);  LcdString("Scroll Demo");} void loop(void){  gotoXY(4,4);  Scroll("Scrolling Message from www.elechouse.com");  delay(200);}