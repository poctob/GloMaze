#include <ST7565.h>

/*
 Laser maze controller.  Configured for Arduino Mega, actual hardware Seeeduino Mega.
 ****
 Version history:
 
 11 Feburary 2014 Alex P. adding display support, adding multiple tries.
 31 March 2013 - Alex P.  initial version
 */
 
#include <SPI.h>        //SPI.h must be included as DMD is written by SPI (the IDE complains otherwise)

#include <TimerOne.h>   //
#include <DMD.h>
#include "SystemFont5x7.h"
#include "Arial_black_16.h"
#include "Arial14.h"



//Fire up the DMD library as dmd
#define DISPLAYS_ACROSS 1
#define DISPLAYS_DOWN 1
#define DISPLAYS_BPP 1
DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN, DISPLAYS_BPP);

//Maximum number of tries
#define MAX_TRIES 5

//Maximum score
#define MAX_SCORE 1000

#define LINE_MAX 8
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

//initial voltage
int int_volt=0;

//flag to determine if the game is running
volatile boolean running=false;

//indicator for testing purposes
int led = 13;

/************************************************************************************
Try counter
************************************************************************************/
int current_try = 1;

/************************************************************************************
Global score value
************************************************************************************/
int current_score = 0;

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
  if(millis()<target_time)
  {
    dmd.scanDisplayBySPI();
  }
  else
  {
    Timer1.detachInterrupt();
  }
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

  //Attach an interrupt to power button. 
  digitalWrite(power_button, LOW); 
  pinMode(medium_button, LOW); 
  pinMode(hard_button, LOW); 
  pinMode(disable_button, LOW); 
  digitalWrite(led, LOW); 
  attachInterrupt(2, start_game, RISING);     
  
  //take initial voltage reading
  int_volt=analogRead(A0);
  print_debug(" Complete!");
  print_debug(" Waiting for input!");

  //clear/init the DMD pixels held in RAM
  dmd.clearScreen( true );   //true is normal (all pixels off), false is negative (all pixels on
  
}

// the loop routine runs over and over again forever:
void loop() {
  target_time = millis()+7000;
  
  Timer1.initialize( 5000 );           //period in microseconds to call ScanDMD. Anything longer than 5000 (5ms) and you can see flicker.
  Timer1.attachInterrupt( ScanDMD );   //attach the Timer1 interrupt to ScanDMD which goes to dmd.scanDisplayBySPI()

  printMarquee("Your Score: "+current_score,5000);
  
  current_score = 0;
  current_try = 1;
  level_select();    
}

/**
*  Waits for the level select to take place.
*  Once selection is made, The proper level is activated.
*/
void level_select()
{
    print_debug(" Waiting for level!");
    boolean selected=false;
    digitalWrite(led, HIGH);
    while (!selected)
    {
       if(digitalRead(easy_button))
       {
         print_debug(" Easy pressed!");
         digitalWrite(level1_power, LOW);         
       }
       else if(digitalRead(medium_button))
       {
         print_debug(" Medium pressed!");
         digitalWrite(level1_power, LOW);
         digitalWrite(level2_power, LOW);
       }
       else if(digitalRead(hard_button))
       {
         print_debug(" Hard pressed!");
         digitalWrite(level1_power, LOW);
         digitalWrite(level2_power, LOW);
         digitalWrite(level3_power, LOW);
       }
       digitalWrite(led, LOW);
       selected=true;
       protect();
    }
}

/**
*  Initializes and monitors the sensors
*/
void protect()
{
  sound_ready();
  int_volt=analogRead(volt_sensor_1)+analogRead(volt_sensor_2)+analogRead(volt_sensor_3);
  int current_value=0;
 
  char sensorvalue[4];
  char strval[14];
  strcpy(strval," Voltage: "); 
  itoa(int_volt, sensorvalue, 10);
  print_debug(strcat(strval, sensorvalue)); 
  
  //check of the value is half of the initial, if it is, sound alarm.
  while(running && digitalRead(disable_button)==LOW)
  {
    current_value=analogRead(volt_sensor_1)+analogRead(volt_sensor_2)+analogRead(volt_sensor_3);
    if(current_value < int_volt/2)
    {       
       print_debug("Alarm!");
       
       if(current_try < MAX_TRIES)
       {
           flicker(current_try, 1000);
           current_try++;
       }
       else
       {
         alarm();       
       }
    }
  }
  if(digitalRead(disable_button)==HIGH)
  {
     print_debug("Stop Pressed!");
  }
  stop_run();
}

/**
* Flickers the lasers
*/
void flicker( int p_times, int p_delay)
{
    for(int i=0; i<p_times;i++)
    {
         digitalWrite(level1_power, HIGH);
         delay(p_delay);
         digitalWrite(level1_power, LOW);
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
    running=false;
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
  running=true;
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
  stop_run();
  digitalWrite(siren_power, LOW);
  digitalWrite(beacon_power, LOW);
  delay(3000);
  digitalWrite(siren_power, HIGH);
  digitalWrite(beacon_power, HIGH);
  
  current_score = 1000 * current_try;
  
}

void printMarquee(String message, int in_delay)
{
   dmd.clearScreen( true );
   dmd.selectFont(Arial_Black_16);
   
   char buf[message.length()+1];
   message.toCharArray(buf, message.length()+1);
   dmd.drawMarquee(buf,message.length()+1,(32*DISPLAYS_ACROSS)-1,0,WHITE,BLACK);
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
