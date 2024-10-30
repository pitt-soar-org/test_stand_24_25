#include <Wire.h>
#include <SPI.h>
#include <MCP3202.h>  //force sensor library 
#include <SD.h>
#include <RadioLib.h> //lora library for rf
#include <string.h>
//pin setup
#define MOSFET_PIN 4
#define LED 5
#define launchPin 1 //button connected to launch
#define SD_DET 10
#define HARDWARE_LED 6 //shows that hardware powered on 
#define launchoverCMD 1 //manuallaunch over pin 


//example set up from radiolib
// NSS pin:   10
// DIO1 pin:  2
// NRST pin:  3
// BUSY pin:  9
SX1262 lora = new Module(10,2,3,9);
//Force sensor
MCP3202 adc= MCP3202(10); //change pin later


//Timer
elapsedMicros timeStamp;

//LOG TIME DELAY 
const unsigned long loopdelay=100;
//File
File dataFile; 

//data variables 
float forceRaw;
String 



//state of the rocket
const int STATE_IDLE = 0;
const int STATE_PREPARE = 1;
const int STATE_IGNITION = 2;
const int STATE_SHUTDOWN = 3;
int currentState = STATE_IDLE;


volatile bool Mosfetwentoff=false;

unsigned long startTime,endTime,elapsedTime; //time variables for micros funct


void setup() 
{
  Serial.begin(115200);
  Wire.begin();

  adc.begin();

  //led to show test stand is on
  pinMode(HARDWARE_LED, OUTPUT);
  digitalWrite(HARDWARE_LED, HIGH); 

  //delay(1000);
  //digitalWrite(HARDWARE_LED, LOW);

  //manual launch over 
  pinMode(launchoverCMD, INPUT);
  digitalWrite(launchoverCMD, LOW); 

  //sx1262 "lora" set up 
  int state = lora.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("LoRa success!"));
  } else {
    Serial.print(F("LoRa failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }
  //what ever leds we want to show the state of the rocket 
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, LOW);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  //checking power up, battery voltage
  //vbat =analogRead(batteryPin)

  //sd setup
  if (!SD.begin(SD_DET)) 
  {
        Serial.println("SD card initialization failed.");
        return; //breaking code
    }
    Serial.println("SD card initialized.");

  dataFile = SD.open("DATAFILE.txt", FILE_WRITE); //change the txt to change the name of the file
  if (dataFile) 
  {   
        Serial.println("SD Card File Found and Created");
        dataFile.println("Time(us),ForceRaw,PressureRaw,RelayValue");
    } else {
        Serial.println("Failed to open .txt for writing");
  }
  while(digitalRead(launchPin)==HIGH); //waits till laucnh pin is active

  
}

void loop() 
{
  startTime=micros();



  switch(currentState)
  {
      case STATE_IDLE:
        if (digitalRead(launchPin) == HIGH) 
        {
        currentState = STATE_PREPARE;
        Serial.println("Switching to PREPARE state.");
        }
        break;
      case STATE_PREPARE:
        if (digitalRead(launchPin) == HIGH) 
        {
        currentState = STATE_IGNITION;
        Serial.println("Switching to IGNITION state.");
        }
        break;
      case STATE_IGNITION:
        // ignite rocket
        if(!Mosfetwentoff)
        {
        digitalWrite(MOSFET_PIN, HIGH); //mosfet pin high
        delay(1000); 
        digitalWrite(MOSFET_PIN, LOW);
        Mosfetwentoff=true;
        }

        forceRaw = readForceSensor();
        FC_Data = readtelementry();
        logData(forceRaw,FC_Data,digitalRead(MOSFET_PIN),currentState);
        
        break; 
      case STATE_SHUTDOWN:
        Serial.println("Flight is now over!");
        if(digitalRead(MOSFET_PIN)== LOW && currentState==STATE_SHUTDOWN) //ensuring we dont lose data if something goes wrong 
        {
        dataFile.close();
        return; //should stop loop
        }         
        break;
      default:
        Serial.println("Went to Default Case");  
        return;
        break;
      }

  //incorporate log data 
  
  endTime=micros();
  elapsedTime=endTime-startTime;


  if (elapsedTime>=10000000) //after 10 seconds burnout time 
  {
      currentState = STATE_SHUTDOWN; //changes to shut down state
      Serial.println("Switching to SHUTDOWN state.");
  }
  if (digitalRead(launchoverCMD) == HIGH)
  {
    currentState = STATE_SHUTDOWN; //changes to shut down state
    Serial.println("Switching to SHUTDOWN state.");
  }
  Serial.print("Elapsed time: ");
  Serial.print(elapsedTime);
  Serial.println(" us");

  delay(loopdelay); //time in between each data log CHANGE
}




void logData(float forceRaw, String FC_data, int mosfetState,int currentState) 
{
  if (dataFile) 
  {
    dataFile.print(micros());   //File would look like (time, forceraw, telem string, mosfet: state)
    dataFile.print(",");
    dataFile.print(forceRaw);
    dataFile.print(",");
    dataFile.print(FC_data);
    dataFile.print(", Mosfet: ");
    dataFile.print(mosfetState);
    dataFile.print(",");
    dataFile.println(currentState);
    dataFile.flush(); //way to ensure data to be written into the sd card may slow down program reconsider this later 
  }
}

String readtelementry()
{
  String FlightComputerData; //this is where the data will be stored 
  int state = lora.receive(FlightComputerData);
  

  if (state == RADIOLIB_ERR_NONE)
  {
      Serial.print("Received data: "); // Check for successful reception
      return FlightComputerData;

  }else if(state!=RADIOLIB_ERR_RX_TIMEOUT) // Print any error except timeout
  {
      Serial.print("LoRa reception failed, code ");
      Serial.println(state);
      
  }
  return "";
} 



float readForceSensor() //load cell sensor data
{
  int rawValue = adc.readChannel(0); // assuming channel 0 for force sensor
  float force = (rawValue/ 4096.0) * 3.3; 
  return force;
}





