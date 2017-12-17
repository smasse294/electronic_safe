/*
 * Shawn Masse
 * 2017-Nov-11
 * Rev 1.1 - Added Backlight signaling
 * 
 * 2017-Nov-5
 * Rev 1.0 - Initial
 * 
 * Pinout - LCD
 * VSS/GND  >   GND
 * VCC/VSS  >   5V
 * VO       >   10K Pot Wiper
 * RS       >   Pin 12 (Arduino)
 * RW       >   GND
 * EN       >   Pin 11 (Arduino)
 * D0       >   NC (Not Connected)
 * D1       >   NC
 * D2       >   NC
 * D3       >   NC
 * D4       >   Pin 5 (Arduino)
 * D5       >   Pin 4 (Arduino)
 * D6       >   Pin 3 (Arduino)
 * D7       >   Pin 2 (Arduino)
 * A        >   Pin 6 (Arduino)
 * K        >   GND (Include inline resistor if not built into display)
 * 
 * Pinout - 10k Pot
 * Wiper    >   VO (LCD)
 * Rest     >   GND, 5V
 * 
 * Arduino recieves data over I2C/TWI
 * Connect the SDA and SCL pins accordingly to their respetive lines
 * 10K Pullup resistors are recomend if more than 1 Slave device is
 * included in the mix.
 * 
 * This program gets characters over the I2C bus and then writes them on screen
 * Some characters are used for special cases. These include:
 *    \r (Carriage Return) > New Line on Display
 *    0 > Locked Symbol
 *    1 > Unlocked Symbol
 *    2 > Turn on backlight for this message
 *    
 * The display backlight is controlled as well and is set up with a timer
 * based on the millis() function and a set amount of time.  It resets when
 * data is recieved over the I2C
 */

//Libraries 
#include <Wire.h>           //I2C/TWI library to recieve input
#include <LiquidCrystal.h>  //LCD library to drive the display

//Defining Pinouts and other factors.  Update these if
//needed due to differing hardware or pinouts

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;  //LCD Pinout
const int i2caddr = 8; //I2C Address
const int lcdcol = 16; //LCD Column Count, 16 & 20 common
const int lcdrow = 2; //LCD Row Count, 2 & 4 common
const int backlight = 6; //Pin to output to the Backlight

//Creates an instance of the LCD
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//Custom Character for a Locked Padlock
byte locked[8] = {
  B00100,
  B01010,
  B10001,
  B10001,
  B11111,
  B11011,
  B11011,
  B11111,
};

//Custom Character for an Unlocked Padlock
byte unlocked[8] = {
  B00100,
  B01010,
  B00001,
  B00001,
  B11111,
  B11011,
  B11011,
  B11111,
};

unsigned long timesince = 0; //Variable to keep track the passage of time while idle
                             //Used to turn off the backlight after a period of time
int timeout = 5000;          //Amount of time before turning off the Backlight (in Miiliseconds)

void setup() {
  Wire.begin(i2caddr);          //join i2c bus at the defined address
  Wire.onReceive(receiveEvent); //Triggers the receiveEvent loop when data is recieved over I2C
  //Serial.begin(9600);           //start serial for output - Debugging Only
  lcd.begin(lcdcol, lcdrow);    //Starts the LCD Instance from Above
  lcd.createChar(0, locked);    //Assigns the Custom Character Above to 0 & 1 Respectively
  lcd.createChar(1, unlocked);  
  pinMode(backlight, OUTPUT);    //Establishing the Pin for the backlight as an output
  digitalWrite(backlight, HIGH); //Turning the backlight on
}

void loop() {
  if(timesince + timeout < millis()){ //testing to see if the backlight has timed out
    digitalWrite(backlight,LOW);      //If so, turn off backlight
  }
  
}

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int howMany) {
  lcd.clear();                   //Clear the Previous Screen of Data
  while (0 < Wire.available()) { // loop through all data recieved over I2C
    char c = Wire.read();        // receive byte as a character
    //Serial.print(c);             //Serial Print the Character - debugging Only
    if (c == 13){                //Look for a Carriage Return \r and treat this
      lcd.setCursor(0,1);        //as a new line on the screen. (Written for 2 Line displays)
      //lcd.write(c);            //Print's Carriage Return on Screen - debugging only
    }
    else if (c == 2){            // Look for character #2 (Wire.write(2);) not the #2 (Wire.write("2");)
      delay(15);
      digitalWrite(backlight, HIGH); //If found, turn the backlight on. This allows the main controller
      delay(5);
      digitalWrite(backlight, HIGH);
      timesince = millis();          //to send data (such as the time) without turning the backlight on. ;
      //delay(5);
    }
    else{             
      lcd.write(c);               //Print characters on the LCD, intended to be displayed
    }
  }
  //Serial.print("\n");             //Print New Line on Serial Monitor for Formatting - Debugging Only
  
   

}
