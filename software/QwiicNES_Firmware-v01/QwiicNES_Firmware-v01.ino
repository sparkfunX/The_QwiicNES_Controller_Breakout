#include <Wire.h>
#include <EEPROM.h>
#include <Joystick.h> //https://github.com/MHeironimus/ArduinoJoystickLibrary/tree/version-2.0

// NES Controller Pins
#define NESCLOCK          11
#define NESLATCH          12
#define NESDATA           8
#define ZAPTRIG           6
#define ZAPSENS           4

// I2C Stuff
#define REG_MAP_SIZE      4
#define MAX_SENT_BYTES    3
#define DEFAULT_ADDR      0x54

// Misc global vars
byte registerMap[REG_MAP_SIZE];
byte receivedCommands[MAX_SENT_BYTES];
byte ctrlButtons = 0;     
byte wire_addr = DEFAULT_ADDR;
uint8_t ledPin[] = {13,5,A0,A1,A5,A4,A3,A2,9,10};
uint16_t gamepadTimer = 0;

// Joystick initial state variables
Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID,JOYSTICK_TYPE_GAMEPAD,
  4, 0,                  // Button Count, Hat Switch Count
  true, true, false,     // X and Y, but no Z Axis
  false, false, false,   // No Rx, Ry, or Rz
  false, false,          // No rudder or throttle
  false, false, false);  // No accelerator, brake, or steering
  
void setup() 
{
  //Check EEPROM for a slave address
  //If no address is stored, set to "factory default"
  byte eepromVal;
  EEPROM.get(0, eepromVal);
  if (eepromVal == 0xFF) {
    EEPROM.put(0, DEFAULT_ADDR);
  } else {
    //If there is an address in EEPROM, set it
    wire_addr = eepromVal;
  }  

  // Set the initial pin states
  pinMode(NESDATA, INPUT);
  pinMode(NESCLOCK, OUTPUT);
  pinMode(NESLATCH, OUTPUT);
  digitalWrite(NESCLOCK, 0);
  digitalWrite(NESLATCH, 0);

  // Start I2C
  Wire.begin(wire_addr); 
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  for (int i = 0; i < 10; i++) {
    pinMode(ledPin[i], OUTPUT);      
    digitalWrite(ledPin[i], 0);  
  }  

  // Clear the controller state accumulator
  registerMap[1] = 0b00000000;
  
}

void loop() 
{
  ctrlButtons = readController(); // Read the current controller state
  setLEDs(ctrlButtons); // Set onboard GPIO/LED states
  registerMap[1] = ctrlButtons | registerMap[1]; // Accumulate controller states since last read

  // Check if SELECT is being held
  if(bitRead(ctrlButtons,2))
  {
    gamepadTimer++;
  }else{
    gamepadTimer = 0;
  }
  if(gamepadTimer>2000){gamepadMode();} // If SELECT is held long enough, launch gamepad mode
}

/****************************************************
  Set LED/GPIO states given the controller state
****************************************************/
void setLEDs(byte buttonStates)
{
  for (int i = 0; i < 8; i++) {
    digitalWrite(ledPin[i], bitRead(buttonStates,i));  
  }
}

/****************************************************
  Read one byte from the NES controller
****************************************************/
byte readController() 
{  
  byte inputs = 0b11111111;
  digitalWrite(NESLATCH, 1);
  digitalWrite(NESLATCH, 0);
  for (int i = 0; i < 8; i++) {
    inputs -= pow(2,i)*digitalRead(NESDATA);
    digitalWrite(NESCLOCK, 1);
    digitalWrite(NESCLOCK, 0);
  }
  return inputs;
}

/****************************************************
  Gamepad Mode
****************************************************/
void gamepadMode()
{
  bool enabled = true;
  for(byte i = 0; i < 4; i++){ 
    for (int i = 0; i < 10; i++) {
      digitalWrite(ledPin[i], 1);  
    }
    delay(250);
    for (int i = 0; i < 10; i++) {
      digitalWrite(ledPin[i], 0);  
    }
    delay(250);
  }

  Joystick.begin();
  Joystick.setXAxisRange(-1, 1);
  Joystick.setYAxisRange(-1, 1);

  int16_t xAxis = 0;
  int16_t yAxis = 0;
  while(enabled)
  {
    xAxis = 0;
    yAxis = 0;
    ctrlButtons = readController(); // Read the current controller state
    if(bitRead(ctrlButtons, 4)){
      yAxis = -1;
    }else if(bitRead(ctrlButtons, 5)){
      yAxis = 1;
    }
    Joystick.setYAxis(yAxis);

    if(bitRead(ctrlButtons, 6)){
      xAxis = -1;
    }else if(bitRead(ctrlButtons, 7)){
      xAxis = 1;
    }
    Joystick.setXAxis(xAxis);

    Joystick.setButton(0, bitRead(ctrlButtons, 0));
    Joystick.setButton(1, bitRead(ctrlButtons, 1));
    Joystick.setButton(2, bitRead(ctrlButtons, 2));
    Joystick.setButton(3, bitRead(ctrlButtons, 3));

   if(bitRead(ctrlButtons,2))
    {gamepadTimer++;}else{gamepadTimer = 0;}
   if(gamepadTimer>1000){enabled = false;}   
   
  }

  Joystick.end();
  
  for(byte i = 0; i < 4; i++){ 
    for (int i = 0; i < 10; i++) {
      digitalWrite(ledPin[i], 1);  
    }
    delay(250);
    for (int i = 0; i < 10; i++) {
      digitalWrite(ledPin[i], 0);  
    }
    delay(250);
  }
  
}

/****************************************************
  Handle requests from I2C Controller
****************************************************/
void requestEvent()
{

  registerMap[0] = ctrlButtons;

  switch (receivedCommands[0]) {

    case 0x00:
      Wire.write(registerMap[0]);
      break;

    case 0x01:
      Wire.write(registerMap[1]);
      registerMap[1] = 0b00000000;
      break;

    case 0x02:
      byte eepromVal;
      EEPROM.get(0, eepromVal);
      Wire.write(eepromVal);
      break;

    default:
      break;

  }
}

/****************************************************
  Handle incoming bytes from I2C Controller
****************************************************/
void receiveEvent(int bytesReceived)
{

/* If we receive a single byte, we can assume the controller is asking for the contents of a register 
   so we store the byte in a global var that we can retreive during requestEvent() */

  for (int a = 0; a < bytesReceived; a++)
  {
    if ( a < MAX_SENT_BYTES)
    {
      receivedCommands[a] = Wire.read(); //grab stuff and jam it into the command buffer
    }
    else
    {
      Wire.read(); //if we receive more data than allowed, chuck it
    }
  }

  if (bytesReceived == 1 && (receivedCommands[0] < REG_MAP_SIZE)) //if we got a byte within the register map range, keep it
  {
    return;
  }

  if (bytesReceived == 1 && (receivedCommands[0] >= REG_MAP_SIZE)) //if we got a byte outside the register map range, chuck it
  {
    receivedCommands[0] = 0x00;
    return;
  }

/* If we receive multiple bytes, the first is a command and the following are arguments so we just handle it */

  // When we receive 0x03, change our I2C address
  if (bytesReceived > 1 && (receivedCommands[0] == 0x03)) {

    if (receivedCommands[1] < 0x80 && receivedCommands[1] > 0x00) {
      EEPROM.put(0, receivedCommands[1]);

      // restart I2C
      Wire.end();
      Wire.begin(receivedCommands[1]); 
      Wire.onRequest(requestEvent);
      Wire.onReceive(receiveEvent);
    }
    receivedCommands[0] = 0x00;
    receivedCommands[1] = 0x00;
    return;
  }

  return; // ignore anything else and return

}
