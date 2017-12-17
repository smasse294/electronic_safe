/*Overview
 * Shawn Masse
 * 2017-11-13
 * Version 1.0 - Initial
 * 
 * This is the main code for an Arduino based Electronic safe.  It makes use of a Matrix Keypad for inputs, a Real Time Clock for time management,
 * an SD card module to record events, and a servo to lock and unlock the safe.  Due to componenty limitations an LCD could not be handled by this 
 * sketch. This was primary due to pin limitations as there is only three free on the Arduino and the LCD requires a minimum of seven.  As such this
 * sketch sends data over the I2C bus to a second Arduino code which controlls the LCD. An I2C backpack may be swapped out for the second Arduino,
 * barring the limmited space remaining causing issues. Since this is the only thing using the I2C it should be easy to modify this sketch for use
 * with an LCD Backpack. This sketch is based on using a DS1302 Real Time Clock. Using a different Real Time Clock should be a simple switch of
 * the library and tweaking a few lines of code.
 * 
 * Pinout:
 * A4 & A5 (SPI) to A4 & A5 on the LCD Controller
 * A3 > Keypad Row 1 (Top)
 * A2 > Keypad Row 2
 * A1 > Keypad Row 3
 * A0 > Keypad Row 4 (Bottom)
 * 7 > Keypad Column 1 (Left)
 * 8 > Keypad Column 2 (Center)
 * 9 > Keypad Column 3 (Right)
 * 6 > Real Time Clock - Chip Enable (Reset)
 * 5 > Real Time Clock - Data
 * 4 > Real Time Clock - Clock
 * 3 > Servo (PWM)
 * 10 > SD Card - Chip Select
 * 11 > SPI (MOSI) (SD Card)
 * 12 > SPI (MISO) (SD Card)
 * 13 > SPI (SCK) (SD Card)
 * All components share a common Ground and common 5V.
 */

/*Setup:  Time
 * This provides the global setup for handling the time aspectsof the project.  There are three key pieces at play.  The Time library, The DS1302RTC
 * library, and the Timezone library. The Time library, <Time.h> & <TimeLib.h>, handle the core time functions and are relied upon by the other two
 * libraries.  The DS1302RTC library, <DS1302RTC.h>, facilitates commuincations between the DS1302 Real Time Clock and the Arduino. Besides calling
 * the library, an instance of the clock is set up with the DS1302RTC RTC(CE, DATA, Clock); call. It can be called globally with the RTC.functions(); 
 * The Timezone library, <Timezone.h>, is put out by JChristensen on Github and is available at:  https://github.com/JChristensen/Timezone under a 
 * Creative Commons Attribution-ShareAlike 3.0 license. This works out converting the RTC clock time (set to UTC) to the local time and compensates
 * for Dalylight Savings time. To do this several items are established.  Two global time_t variables are established, one for the UTC time from the
 * real time clock and a second for the local time. Two TimeChangeRule are set up to establish when Daylight Savings Time begins (myDST) and when it
 * ends (mySTD). This is established by passing the following related to the timezone and change to/from Daylight Savings Time:  
 * {Timezone abbreviation, Week of Month, Day of Week, Month, Hour of Change, offset from UTC}. A third TimeChangeRule is used as pointer for the
 * library. Finally the TimeChangeRules are passed to the created instance with the Timezone myTZ(myDST, mySTD); call.
 */
#include <Time.h>
#include <TimeLib.h>
#include <DS1302RTC.h>
#include <Timezone.h>
DS1302RTC RTC(6, 5, 4);
time_t utc, local, lastact;
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};
TimeChangeRule *tcr;
Timezone myTZ(myDST, mySTD);

/*Setup:  Keypad
 * The Keypad library handles most of the work of checking for key presses, handling special cases, and providing key presses to the rest of the code.  
 * Setup requires the defining of the number of rows and columns of the keypad (const byte ROWS & COLS), laying out the keys (char keys), and defining
 * the inputs (byte rowPins[ROWS] & byte colPins[COLS]). With everything defined, a single instance is created that puts everything together.  Beyond 
 * that, it is easy and simple to get keypresses and process the input.
 */
#include <Keypad.h>
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
    {'1','2','3'},
    {'4','5','6'},
    {'7','8','9'},
    {'*','0','#'}
};
byte rowPins[ROWS] = {A3, A2, A1, A0};
byte colPins[COLS] = {7, 8, 9};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

/*Setup:  LCD
 * Since the LCD is driven by a seperate Arduino, the only element needed in this sketch is the Wire library to facility communciation between the
 * two over the I2C bus.
 */
#include <Wire.h>

/*Setup:  SD Card
 * This provides global setup for the SD card. This relies on the standard SD card library, <SD.h>, for handling writing to the SD card.  
 * In addition, the SPI library, <SPI.h> is used to handle communications between the SD card module and the Arduino.  
 */
#include <SPI.h>
#include <SD.h>
File logfile;

/*Setup:  Authentication Methods
 * There are two authentication methods at play in this sketch. A password and a Time Based One-Time Password. Two boolean variables (auth1 & auth2) 
 * keep track which authentication methods have or have not been met. The rest of this setup support the Time Based One Time Password.  The TOTP 
 * library, <TOTP.h>, is put out by lucadentella on Github and is available at:  https://github.com/lucadentella/TOTP-Arduino. This library also relies    
 * on the sha1 library <sha1.h>. For this library to function a key is needed.  The generated key is stored with the uint8_t OTPKey[]. An instatnce is 
 * created and fed the key (TOTP totp = TOTP(OTPKey, 10)).  Finally thre variable are used to accept the generated one time password (char* totpCode),
 * the user enter password/one time password (char inputCode[7]), and to keep track of the key input count (int inputCode_idx).
 */

#include <sha1.h>
#include <TOTP.h>
uint8_t OTPKey[] = {0x53, 0x61, 0x66, 0x65, 0x44, 0x65, 0x6d, 0x6f, 0x30, 0x30};
TOTP totp = TOTP(OTPKey, 10);
char* totpCode;
char inputCode [7];
int inputCode_idx;
boolean auth1 = false;
boolean auth2 = false;

/*Setup:  Servo
 * The servo setup relies on the Servo library (<Servo.h>)that comes with the Arduino IDE. Other than including the library in the sketch, a single 
 * instance is needed (Servo lockservo). To use the servo, the servo is attached, with the PWM pin passed along (lockservo.attach(3)). A write 
 * command can then be used to indicate, in degrees, the position desired (lockservo.write(pposition)). Since the servo is not needed all the time,
 * the servo is attached, written two, a dealy of 1 second is used (delay(1000)) following by a detach command.  This allows the servo to operate
 * and keep the noise level down (the MG-996R servo being used in testing produce a fair amount of noise while attached).
 */
#include <Servo.h>
Servo lockservo;

/*setup()
 * This is the setup function of the sketch. Since most of the setup is handled at the top of the code, there is little that is needed in this portion
 * What does need to be setupt is starting the SD card (SD.begin(10)), Starting the I2C bus (Wire.begin()), start Sync Provider 
 * (setSyncProvider(RTC.get)), capture the current time, ensuring the servo is in a know (locked) state, and start the Keypad Event 
 * (keypad.addEventListener(keypadEvent)).  The Sync Provider keeps the time synced betweent the Real Time Clock and the Arduino. The Keypad Event 
 * listener helps with caputring keypresses and parsing themn to the rest of the code. 
 */
void setup() {
  Wire.begin();
  setSyncProvider(RTC.get);
  lastact = now();
  SD.begin(10);
  keypad.addEventListener(keypadEvent);  
  lockservo.attach(3);
  lockservo.write(15);
  delay(1000);
  lockservo.detach();
}

/*loop() function
 * The main loop of the function is kept clean by use of several seperate functions that follow. The loop is primary responsible for handling keypresses
 * and determining what function to trigger. There is one variable (char key) that gets the keypress.  There are also two if else statements to sort
 * out the state of the safe.  The first if-else handles the keypress and is triggered when a key is actually pressed (it's skipped when there is no
 * new keypress). This statement adds the keypress to the string of keypresses to buid the code entered, captures the current time, and determines
 * if the code is bening entered for the first or second authentication method with a nested if-else statement.  The second function level if-else
 * statement handles timeout for code entry (5 seconds for password, 2 minutes for OTP).  The timeout also tells the system to display the time
 * while ide.  The passwordcheck() function is called to verfi the password and the authenticate() function is called to check the One Time Password.
 */
void loop() {
  char key = keypad.getKey();
  if (key != NO_KEY) {
    lastact = now();
    inputCode[inputCode_idx++] = key;
    if (inputCode_idx == 6 && auth1 == false) passwordcheck(); 
    else if (inputCode_idx == 6 && auth1 == true && auth2 == false)authenticate();   
  }
  if (auth1 == true && auth2 == false && lastact + 119 < now()){  //lock();
    auth1 = false;
    inputCode_idx = 0;
  }
  else if (auth1 == false && auth2 == false && lastact + 4 < now()) {
    inputCode_idx = 0;
    showtime();
  }
}

/*Function to check the first password/factor
 * This function checks the password against the user inputed code.  At the core is and If-Else statement.  The If statement executes when the password
 * and user input match.  The else statement executes when the password and user input do not match.  Since the keypad output is treated as a string
 * the strcmp is used to compare the two strings (password and user input). This returns a 0 if it matches and a 1 or -1 if they do not. The rest of
 * the statements is just being and print statements to update the LCD and update the SD card.  It also resets the counter for the user input to ready
 * for another attempt or for the Time Based Password depending on the results.
 */
void passwordcheck(){
  if(strcmp(inputCode, "336600") == 0){
    auth1 = true;
    char message[] = "Password Successful\n";
    Wire.beginTransmission(8);
    Wire.write(2);
    Wire.print(message);
    Wire.print("\r");
    Wire.print("Enter OTP:");
    Wire.endTransmission();
    logfile = SD.open("log.txt", FILE_WRITE);
    logfile.print(message);
    logfile.close();
    inputCode_idx = 0;
    writetime();
   }
  else{
    char message[] = "Password Failed";
    Wire.beginTransmission(8);
    Wire.write(2);
    Wire.print(message);
    Wire.endTransmission();
    logfile = SD.open("log.txt", FILE_WRITE);
    logfile.print(message);
    logfile.close();
    writetime();
    inputCode_idx = 0;     
    auth1 = false; 
  }
}

/*Function to authenticate against the Second Factor
 * This function checks the time based password against the user inputed code.  At the core is and If-Else statement.  The If statement executes when 
 * the time base code and the user input match.  The else statement executes when the time based code and user input do not match.  Since the keypad 
 * output and one time code generated by the arduino are treated as a strings the strcmp is used to compare the two strings (time based code and user 
 * input). This returns a 0 if it matches and a 1 or -1 if they do not. The rest of the statements is just being and print statements to update the 
 * LCD and update the SD card.  It also resets the counter for the user input to ready for another attempt or for the password once the safe is 
 * relocked. This function also generates the time based code for comparsion.  To do so it needs the current time in GMT/UTC.   
 */
void authenticate(){
  utc = now();
  totpCode = totp.getCode(utc);
  
  if(strcmp(inputCode, totpCode) == 0){
    auth2 = true;
    char message[] = "Authenticated\rUnlocking Safe";
    Wire.beginTransmission(8);
    Wire.write(2);
    Wire.print("\r");
    Wire.print(message);
    Wire.endTransmission();
    logfile = SD.open("log.txt", FILE_WRITE);
    logfile.print(message);
    logfile.close();
    writetime();
    inputCode_idx = 0;
    unlock(); 
  }
  else{
    auth2=false;
    char message[] = "Authentication\rFailed";
    Wire.beginTransmission(8);
    Wire.write(2);
    Wire.print(message);
    Wire.endTransmission();
    logfile = SD.open("log.txt", FILE_WRITE);
    logfile.print(message);
    logfile.close();
    auth1 = false;
    inputCode_idx = 0;
  }
}

/*Function to Lock Safe
 * This function is triggered when the * button is held down.  It displays that theS afe is Locked and writes that to the SD card.  It also rests
 * auth1 & auth2 so the user will have to reenter the passowrd and code to access the safe. The Servo is attached and returned to the lock positition
 * before being detached again.
 */
void lock(){
  char message[] = "Safe Locked";
  lockservo.attach(3);
  lockservo.write(15);
  delay(1000);
  lockservo.detach();
  Wire.beginTransmission(8);
  Wire.write(2);
  Wire.print(message);
  Wire.endTransmission();
  auth1 = false;
  auth2 = false;
  logfile = SD.open("log.txt", FILE_WRITE);
  logfile.print(message);
  logfile.close();
  writetime();  
}

/*Function to Unlock Safe
 * This function is triggered when the password & one time pasword/code both match user inputs.  It displays that the Safe is unLocked and writes 
 * that to the SD card.  It also displays instructions on how th relock the safe. The servo is attached and set to the unlocked position before
 * being detached again.
 */
void unlock(){
  lockservo.attach(3);
  lockservo.write(165);
  delay(1000);
  lockservo.detach();
  Wire.beginTransmission(8);
  Wire.write(2);
  Wire.print("Safe Unlocked\rHold * to Lock");
  Wire.endTransmission();
  }

/*Keypad Event function feeding the Loop()
 * This function used to capture key events and fees it to the loop. It also holds the special case if someone holds the * button which triggers
 * the Lock() function to lock the safe. 
 */
void keypadEvent(KeypadEvent key){
  switch (keypad.getState()){
  case HOLD:
    if (key == '*'){
      lock();
    }
  }
}

/*showtime() function
 * This is a function to send the current date and time to the LCD. The date and time is formatted Line 1: Day YYYY-MM-DD Lind 2: HH:MM:SS AM/PM.
 * It works by sending data over the I2C to the LCD controlling Arduino. It then uses a set of print statements to print out the necessary elements
 * with a few if-else statements to handle special cases.  These special cases include: converting from 24hr to 12hr, printing leading zeros on
 * the minutes and seconds, and printing AM/PM accordingly. The function begins with updating the time held in global variables (utc and local). 
 * The \r has been set on the LCD side as a second line command. It also updates lastact, so the function is not being constally called in the 
 * loop.  lastact is set in the past so to update the screen every second but allow other functions to use it to trigger the time after the full 
 * amount of time allocated to timing out.
 */
void showtime(){

  utc = now();
  local = myTZ.toLocal(utc, &tcr);
  
  Wire.beginTransmission(8);
  Wire.print(dayShortStr(weekday(local)));
  Wire.print(" ");
  Wire.print(year(local));
  Wire.print("-");
  Wire.print(month(local));
  Wire.print("-");
  Wire.print(day(local));
  Wire.print("\r");
  if(hour(local) > 12) Wire.print(hour(local)-12);
  else Wire.print(hour(local));
  Wire.print(":");
  if(minute(local)<10){
    Wire.print("0");
  }
  Wire.print(minute(local));
  Wire.print(":");
  if(second(local)<10) Wire.print("0");
  Wire.print(second(local));
  if(hour(local) >= 12) Wire.print(" PM");
  else Wire.print(" AM");
  Wire.endTransmission();
  lastact = now() - 4;
}

/*writetime() function
 * This is a function to add the current date and time to the SD card log file. The date and time is formatted Day YYYY-MM-DD [tab] HH:MM:SS AM/PM.
 * It works by opening the file and assinging it to the variable logfile. It than uses a set of print statements to print out the necessary elements
 * with a few if-else statements to handle special cases.  These special cases include: converting from 24hr to 12hr, printing leading zeros on
 * the minutes and seconds, and printing AM/PM accordingly. The function begins with updating the time held in global variables (utc and local). 
 */
void writetime(){

  utc = now();
  local = myTZ.toLocal(utc, &tcr);
  
  logfile = SD.open("log.txt", FILE_WRITE);
  logfile.print("\n");
  logfile.print(dayShortStr(weekday(local)));
  logfile.print(" ");
  logfile.print(year(local));
  logfile.print("-");
  logfile.print(month(local));
  logfile.print("-");
  logfile.print(day(local));
  logfile.print("\t");
  if(hour(local) > 12) logfile.print(hour(local)-12);
  else logfile.print(hour(local));
  logfile.print(":");
  if(minute(local)<10){
    logfile.print("0");
  }
  logfile.print(minute(local));
  logfile.print(":");
  if(second(local)<10) logfile.print("0");
  logfile.print(second(local));
  if(hour(local) >= 12) logfile.print(" PM\n\n");
  else logfile.print(" AM\n\n");
  logfile.close();
}




