#include <ST7565.h>

/*
 Laser maze controller.  Configured for Arduino Mega, actual hardware Seeeduino Mega.
 ****
 Version history:
 
 31 March 2013 - Alex P.  initial version
 */

#include <Arial_black_16.h>
#include <DMD.h>
#include <SystemFont5x7.h>
#include <Arial14.h>

#include <SPI.h>        //SPI.h must be included as DMD is written by SPI (the IDE complains otherwise)

#include <TimerOne.h>   //
#include "SystemFont5x7.h"
#include "Arial_black_16.h"
#include "Arial14.h"


//Fire up the DMD library as dmd
#define DISPLAYS_ACROSS 1
#define DISPLAYS_DOWN 1
DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN);

//Maximum score
#define MAX_SCORE 1000
#define LINE_MAX 8

/**
  multipliers
*/
#define L1_MULTIPLIER 0.5
#define L2_MULTIPLIER 0.25
#define L3_MULTIPLIER 0.25

//Trip delay
#define TRIP_DELAY 0

/************************************************************************************
LCD driver pins 30-34
************************************************************************************/
int sid_pin=30;
int sclk_pin=31;
int rs_pin=32;
int rst_pin=33;
int cs_pin=34;

ST7565 glcd(sid_pin, sclk_pin, rs_pin, rst_pin, cs_pin);
int current_line=0;
/************************************************************************************
Digital input pins 21-37
************************************************************************************/
//Pin 21 used to initialize the system.  Normally this is a button located outside of the maze.
//It's attached on an interrupt 2 pin.
int power_button=21;

//Pin 23-25.  Difficulty level selector + starts the game.
int easy_button=23;
int medium_button=24;
int hard_button=25;

//Pin 26.  Disable button.
int disable_button=26;

/************************************************************************************
Analog input
************************************************************************************/
//Analog pin 0.  This is the voltage sensor for level 1.
int volt_sensor_1=A0;

//Analog pin 1.  This is the voltage sensor for level 2.
int volt_sensor_2=A1;

//Analog pin 2.  This is the voltage sensor for level 3.
int volt_sensor_3=A2;

/************************************************************************************
Output pins 38-63
************************************************************************************/
//pin 38 laser 1 level power relay
int level1_power=38;

//pin 39 laser 2 level power relay
int level2_power=39;

//pin 40 laser 3 level power relay
int level3_power=40;

//pin 41 siren
int siren_power=41;

//pin 42 beacon
int beacon_power=42;

//flag to determine if the game is running
volatile boolean running=false;

//indicator for testing purposes
int led = 13;

//Current level
int current_level=0;

//Possible levels
enum Level
{
  NONE,
  ONE,
  TWO,
  THREE,
  MAX
};

/************************************************************************************
Global score value
************************************************************************************/
int current_score = MAX_SCORE;

/************************************************************************************
timing values
************************************************************************************/
unsigned long target_time = 0;

/*--------------------------------------------------------------------------------------
  Interrupt handler for Timer1 (TimerOne) driven DMD refresh scanning, this gets
  called at the period set in Timer1.initialize();
--------------------------------------------------------------------------------------*/
void ScanDMD()
{ 
  dmd.scanDisplayBySPI();
}

// the setup routine runs once when you press reset:
void setup() {          
  // initialize and set the contrast to 0x18
  glcd.begin(0x18);
  glcd.clear();
  print_debug("Initializing...");
  
  // initialize input digital pins.
  pinMode(power_button, INPUT); 
  pinMode(easy_button, INPUT); 
  pinMode(medium_button, INPUT); 
  pinMode(hard_button, INPUT); 
  pinMode(disable_button, INPUT);   
  
  //initialize output digital pins.
  pinMode(level1_power, OUTPUT); 
  pinMode(level2_power, OUTPUT); 
  pinMode(level3_power, OUTPUT); 
  pinMode(beacon_power, OUTPUT); 
  pinMode(siren_power, OUTPUT); 
  pinMode(led, OUTPUT); 
  
  //Deactivate the relays
  digitalWrite(level1_power, HIGH);
  digitalWrite(level2_power, HIGH);
  digitalWrite(level3_power, HIGH);
  digitalWrite(beacon_power, HIGH);
  digitalWrite(siren_power, HIGH);

  //Attached an interrupt to power button. 
  digitalWrite(power_button, LOW); 
  pinMode(medium_button, LOW); 
  pinMode(hard_button, LOW); 
  pinMode(disable_button, LOW); 
  digitalWrite(led, LOW); 
  attachInterrupt(2, start_game, RISING);     

  print_debug(" Complete!");
  print_debug(" Waiting for input!");
  Timer1.initialize( 5000 );           //period in microseconds to call ScanDMD. Anything longer than 5000 (5ms) and you can see flicker.
 
  
}

// the loop routine runs over and over again forever:
void loop() {
  
  while(!running)
  {
     delay(500);
  }
  
  
  delay(1000);
  String score = String(current_score);
  Timer1.attachInterrupt( ScanDMD ); 
  printMarquee("Intruder Detected! Starting Score: "+score,30);
  Timer1.detachInterrupt();
  current_score = MAX_SCORE;
  level_select();    
}

/**
*  Waits for the level select to take place.
*  Once selection is made, The proper level is activated.
*/
void level_select()
{
    //print_debug(" Start pressed!");
    print_debug(" Waiting for level!");
    boolean selected=false;
    digitalWrite(led, HIGH);
    while (!selected)
    {
      //Press easy button and disable button at the same time for 
      //test mode.
       if(digitalRead(easy_button))
       {
         if(digitalRead(disable_button)==HIGH)
         {
           runTest();
           return;
         }
         print_debug(" Easy pressed!");        
         selected=true;   
         current_level=ONE;
         
       }
       else if(digitalRead(medium_button))
       {
         print_debug(" Medium pressed!");       
         selected=true;
         current_level=TWO;
       }
       else if(digitalRead(hard_button))
       {
         print_debug(" Hard pressed!");
         selected=true;
         current_level=THREE;
       }
       
 
    }
       
       processPower();
       digitalWrite(led, LOW);       
       protect();
       //attachInterrupt(2, start_game, RISING); 
}

/**
* Turns the lasers on based on the currently selected level.
*/
void processPower()
{
  switch (current_level)
  {
    case ONE:
        digitalWrite(level1_power, LOW); 
        break; 
    case TWO:
       digitalWrite(level1_power, LOW);
       digitalWrite(level2_power, LOW);
       break;
    case THREE:
       digitalWrite(level1_power, LOW);
       digitalWrite(level2_power, LOW);
       digitalWrite(level3_power, LOW);
       break;
    default:
      break;    
  }
}


/**
* Checks if sensor has been tripped.
*/
boolean isTripped(int initial1, int initial2, int initial3)
{
  float current_value1=0;
  float current_value2=0;
  float current_value3=0;
  boolean return_value=false;
  
   switch (current_level)
  {
    case ONE:
        current_value1=analogRead(volt_sensor_1);
        return_value=(current_value1 < initial1*L1_MULTIPLIER);
        break; 
    case TWO:
        current_value1=analogRead(volt_sensor_1);
        current_value2=analogRead(volt_sensor_2);
        return_value=(current_value1 < initial1*L1_MULTIPLIER) || 
        (current_value2 < initial2*L2_MULTIPLIER);
       break;
    case THREE:
        current_value1=analogRead(volt_sensor_1);
        current_value2=analogRead(volt_sensor_2);
        current_value3=analogRead(volt_sensor_3);
        return_value=(current_value1 < initial1*L1_MULTIPLIER) || 
        (current_value2 < initial2*L2_MULTIPLIER) ||
        (current_value3 < initial3*L2_MULTIPLIER);
       break;
    default:
      break;    
  }
  return return_value;
}
/**
*  Initializes and monitors the sensors
*/
void protect()
{ 
  delay(500);
  int int_volt1=analogRead(volt_sensor_1);
  int int_volt2=analogRead(volt_sensor_2);
  int int_volt3=analogRead(volt_sensor_3);
   sound_ready();
  
  char sensorvalue[4];
  char strval[14];
  strcpy(strval," Voltage1: "); 
  itoa(int_volt1, sensorvalue, 10);
  print_debug(strcat(strval, sensorvalue)); 
  
  strcpy(strval," Voltage2: "); 
  itoa(int_volt2, sensorvalue, 10);
  print_debug(strcat(strval, sensorvalue)); 
  
  strcpy(strval," Voltage3: "); 
  itoa(int_volt3, sensorvalue, 10);
  print_debug(strcat(strval, sensorvalue)); 
  running=true;
  

  while(running && digitalRead(disable_button)==LOW)
  {
    if(isTripped(int_volt1,int_volt2,int_volt3))
    {       
       delay(TRIP_DELAY);        
       if(isTripped(int_volt1,int_volt2,int_volt3))
       {
          print_debug("Alarm!");
          alarm();    
          //running=false; 
            
       }
    }
  }
  if(digitalRead(disable_button)==HIGH)
  {
     print_debug("Stop Pressed!");
     stop_run();
  }
  
}

/**
*  Interrupts the run.
*/
void stop_run()
{
    print_debug("Terminating...");
    digitalWrite(level1_power, HIGH);
    digitalWrite(level2_power, HIGH);
    digitalWrite(level3_power, HIGH);
   // running=false;
   String score = String(current_score);
  Timer1.attachInterrupt( ScanDMD ); 
  printMarquee("Your Score: "+score,30);
  Timer1.detachInterrupt();
  current_score = MAX_SCORE;
    
}

/**
*  Sounds ready signal.
*/
void sound_ready()
{  
  digitalWrite(beacon_power, LOW);
  delay(1000);
  digitalWrite(beacon_power, HIGH);
  print_debug("Ready...");
}

/**
* Interrupt handler function.  Toggle running flag.
*/
void start_game()
{
  print_debug("Start interrupt...");
  if(!running)
  {
    running=true;
  }
  detachInterrupt(2); 
}

/**
* Displays debug information on the LCD screen
*/
void print_debug(char * message)
{
  if(message)
  {
    if(current_line>=LINE_MAX)
    {
      current_line=0;
      glcd.clear();
    }
    glcd.drawstring(0, current_line, message);
    glcd.display();
    current_line++;
  }
}

//sound alarm
void alarm()
{
  current_score-=50;
  //stop_run();
  //digitalWrite(siren_power, LOW);
  digitalWrite(beacon_power, LOW);
  delay(1000);
  digitalWrite(siren_power, HIGH);
  digitalWrite(beacon_power, HIGH);
  
}

/**
*  Diagnostics mode.  Turns on all lasers and
*  prints out sensor readings on the screen.
*/
void runTest()
{
  digitalWrite(level1_power, LOW);
  digitalWrite(level2_power, LOW);
  digitalWrite(level3_power, LOW);
  
  print_debug("Test Mode");
  delay(1000);
  running=false;
  
  while(digitalRead(disable_button)==LOW)
  {
       
      int int_volt1=analogRead(volt_sensor_1);
      int int_volt2=analogRead(volt_sensor_2);
      int int_volt3=analogRead(volt_sensor_3);
      char sensorvalue[4];
      char strval[14];
      strcpy(strval," Voltage1: "); 
      itoa(int_volt1, sensorvalue, 10);
      print_debug(strcat(strval, sensorvalue)); 
      
      strcpy(strval," Voltage2: "); 
      itoa(int_volt2, sensorvalue, 10);
      print_debug(strcat(strval, sensorvalue)); 
      
      strcpy(strval," Voltage3: "); 
      itoa(int_volt3, sensorvalue, 10);
      print_debug(strcat(strval, sensorvalue)); 
      delay(1000);
  }
  digitalWrite(level1_power, HIGH);
  digitalWrite(level2_power, HIGH);
  digitalWrite(level3_power, HIGH);
  attachInterrupt(2, start_game, RISING); 
    
    
}


void printMarquee(String message, int in_delay)
{
   dmd.clearScreen( true );
   dmd.selectFont(Arial_Black_16);
   
   char buf[message.length()+1];
   message.toCharArray(buf, message.length()+1);
   dmd.drawMarquee(buf,message.length()+1,(32*DISPLAYS_ACROSS)-1,0);
   long start=millis();
   long timer=start;
   boolean ret=false;
   while(!ret){
     if ((timer+in_delay) < millis()) {
       ret=dmd.stepMarquee(-1,0);
       timer=millis();
     }
   }
}
