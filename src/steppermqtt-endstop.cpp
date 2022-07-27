/*
  Blinds stepper motor for ESP32 w/ Endstops
  2022 EricSnis

  It checks the endstop then
  - Steps blinds closed until endstop is reached
  - If endstop is reached then run main program
  It connects to an MQTT server and:
  - checks washing blinds open/closed status (topic blinds/blind1)
    - MQTT subscription can be changed to suit by changing TOPIC1
  - checks current open/closed flag
    - Ignores MQTT command if the flag and the MQTT status are the same
  - opens and closes blinds based on status
  - Polls the open and close blinds and calibration buttons
    - If calibration button is pressed it toggles calibration mode
      - Lights status LED to indicate in calibration mode
      - Open blinds button steps motor toward open 1/4 turn
      - Close blinds button steps motor toward closed 1/4 turn
    - If out of calibration mode
      - Open blinds button rotates motor toward full open
      - Close blinds button rotates motor toward full closed
    - Up & Down also updates the blind status in MQTT
  - Pin definitions
    - Calibration on pin 26
    - Blind 1 Up button on pin 39
    - Blind 1 Down button on pin 36
    - Blind 2 Up button on pin 22
    - Blind 2 Down button on pin 23
    Pins can be changed to suit your particular set up if not using included board design
    - Note: GPIO 6-11 are used for flash memory and cannot be used
    - Note: Internal pullup on GPIO 34-39 is not available on ESP32
      - Can still use but you need to provide a pullup resistor
  - Amount of backlash in the blinds rod can be adjusted
    - Change the blindsBacklash variable
    - eg. Half a turn at 2048 steps per rotation would be 1024

  Stepper driver info
  - Uses ULN2003 based board KS0327 Keyestudio stepper motor drive board
  - The stepper driver is attached to digital pins 9, 13, 12 & 14 of the ESP32.
    - Pins defined as Stepper1In1, Stepper1In2, Stepper1In3, etc.
    - Connect in the order they appear on board i.e. In1, In2, In3, In4
  - Driver board should be connected to 5V source
  - The motor driver is attached to the following pins on the ESP32
    - Driver 1 Input 1 - Arduino 15
    - Driver 1 Input 2 - Arduino 2
    - Driver 1 Input 3 - Arduino 0
    - Driver 1 Input 4 - Arduino 4
    - Driver 2 Input 1 - Arduino 19
    - Driver 2 Input 2 - Arduino 21
    - Driver 2 Input 3 - Arduino 3
    - Driver 2 Input 4 - Arduino 1
    Pins can be changed to suit your particular set up if not using included board design
    Note: GPIO 6-11 are used for flash memory and cannot be used
    Note: Internal pullup on GPIO 34-39 is not available on ESP32

  Assumes use of ESP32 based NodeMCU board, not similarly named ESP8266 NodeMCU.
  - Can be changed to other boards if pinout is changed

  Uses the following libraries
  - Arduino stepper library released under GNU Lesser General Public
  - PubSubClient by Nick O'Leary released under MIT license

  Code released under MIT License
*/

#include <Arduino.h>
#include <Stepper.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define STEPS_PER_REV 2048 // number of steps for one revolution of motor

const int LED_BUILTIN = 2;       // Onboard LED
const int LED_Status1 = 26;      // Status LED 1
const int Btn1_Pin = 39;         // Blind 1 open button
const int Btn2_Pin = 36;         // Blind 1 close button
const int Btn3_Pin = 22;         // Blind 2 open button
const int Btn4_Pin = 23;         // Blind 2 close button
const int Endstop1_Pin = 32;     // Endstop 1 switch
const int Endstop2_Pin = 35;     // Endstop 2 switch
const int CalibrateBtn_Pin = 27; // Calibration button
const int Stepper1In1 = 15;      // Test Input pin 1 of stepper driver 1
const int Stepper1In2 = 2;       // Test Input pin 2 of stepper driver 1
const int Stepper1In3 = 0;       // Test Input pin 3 of stepper driver 1
const int Stepper1In4 = 4;       // Test Input pin 4 of stepper driver 1
const int Stepper2In1 = 19;      // Input pin 1 of stepper driver 2
const int Stepper2In2 = 21;      // Input pin 2 of stepper driver 2
const int Stepper2In3 = 3;       // Input pin 3 of stepper driver 2
const int Stepper2In4 = 1;       // Input pin 4 of stepper driver 2

const char *ssid = "WiFiSSID";         // name of your WiFi network
const char *password = "WiFiPassword"; // password of the WiFi network

const char *ID = "Blind1";                   // Hostname of our device, must be unique
const char *TOPIC1 = "Blind1/positionState"; // MQTT topic for blind 1
const char *TOPIC2 = "Blind2/positionState"; // MQTT topic for blind 2

const char *openCmd = "UP";    // Command for opening blinds from MQTT
const char *closeCmd = "DOWN"; // Command for opening blinds from MQTT

IPAddress broker(192, 168, 1, 1); // IP address of your MQTT server
WiFiClient wclient;               // Setup WiFi client

PubSubClient client(wclient); // Setup MQTT client

const int BlindsRevs = 3;        // How many revolutions to open/close blinds
const int blindsBacklash = 1024; // How much backlash in the blinds rod in steps
const int stepperSpeed = 10;     // Speed of motor in RPM

Stepper stepper1(STEPS_PER_REV, Stepper1In1, Stepper1In3, Stepper1In2, Stepper1In4); // Create instance of stepper for stepper 1
Stepper stepper2(STEPS_PER_REV, Stepper2In1, Stepper2In3, Stepper2In2, Stepper2In4); // Create instance of stepper for stepper 2

bool blind1Open = false;      // Blind 1 open flag
bool blind2Open = false;      // Blind 2 open flag
bool calibrationMode = false; // Calibration mode flag

int stepCountBlind1 = 0; // number of steps the motor has taken
int stepCountBlind2 = 0; // number of steps the motor has taken

void blinkled()
{
  digitalWrite(LED_BUILTIN, HIGH); // LED ON
  delay(250);
  digitalWrite(LED_BUILTIN, LOW); // LED OFF
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH); // LED ON
  delay(250);
  digitalWrite(LED_BUILTIN, LOW); // LED OFF
  delay(100);
}

void motorStop()
{
  Serial.println("Stopping motor...");
  digitalWrite(Stepper1In1, LOW);
  digitalWrite(Stepper1In2, LOW);
  digitalWrite(Stepper1In3, LOW);
  digitalWrite(Stepper1In4, LOW);
}

void openBlinds(int step)
{
  switch (step)
  {
  case 1:
    Serial.println("Opening Blind 1...");
    stepper1.step(STEPS_PER_REV * BlindsRevs);
    blind1Open = true; // Set blinds open flag
    stepCountBlind1 = (stepCountBlind1 + (STEPS_PER_REV * BlindsRevs));
    motorStop();
    client.publish(TOPIC1, "OPEN");
    return;
  case 2:
    Serial.println("Opening Blind 2...");
    stepper2.step(STEPS_PER_REV * BlindsRevs);
    blind2Open = true; // Set blinds open flag
    stepCountBlind2 = (stepCountBlind2 + (STEPS_PER_REV * BlindsRevs));
    motorStop();
    client.publish(TOPIC2, "OPEN");
    return;
  }
}

void closeBlinds(int step)
{
  switch (step)
  {
  case 1:
    Serial.println("Closing Blind 1...");
    stepper1.step((-STEPS_PER_REV * BlindsRevs) - blindsBacklash);
    blind1Open = false; // Set blinds open flag
    stepCountBlind1 = 0;
    motorStop();
    client.publish(TOPIC1, "CLOSED");
    return;
  case 2:
    Serial.println("Closing Blind 2...");
    stepper2.step((-STEPS_PER_REV * BlindsRevs) - blindsBacklash);
    blind2Open = false; // Set blinds open flag
    stepCountBlind2 = 0;
    motorStop();
    client.publish(TOPIC2, "CLOSED");
    return;
  }
}

void stepopen(int step)
{
  switch (step)
  {
  case 1:
    Serial.println("Stepping open blind 1..");
    stepper1.step(STEPS_PER_REV / 4);
    motorStop();
  case 2:
    Serial.println("Stepping open blind 2..");
    stepper2.step(STEPS_PER_REV / 4);
    motorStop();
  }
}

void stepclosed(int step, int mult = 4) // Step close specified stepper
{
  int stepsToTake = 100; // Base number of steps
  switch (step)
  {
  case 1:
    Serial.println("Stepping close blind 1..");
    stepper1.step(-stepsToTake * mult); // Step close 100 steps times multiplier, mult defaults to 4
    motorStop();
  case 2:
    Serial.println("Stepping close blind 2..");
    stepper2.step(-stepsToTake * mult); // Step close 100 steps times multiplier, mult defaults to 4
    motorStop();
  }
}

void startupCalibration()
{
  Serial.println("Startup Calibration");
  pinMode(Endstop1_Pin, INPUT_PULLUP);      // Blind 1 endstop as input with pullup
  pinMode(Endstop2_Pin, INPUT_PULLUP);      // Blind 2 endstop as input with pullup
  int endStop1 = digitalRead(Endstop1_Pin); // Read blind 1 endstop switch
  int endStop2 = digitalRead(Endstop2_Pin); // Read blind 2 endstop switch
  while (endStop1)                          // If blind 1 is not closed
  {
    Serial.println("Blind 1 not closed yet");
    stepclosed(1, 1);                     // Step close blind 1/switch not closed
    endStop1 = digitalRead(Endstop1_Pin); // Check the endstop switch
  }
  while (endStop2) // If blind 2 is not closed/switch not closed
  {
    Serial.println("Blind 2 not closed yet");
    stepclosed(2, 1);                     // Step close blind 2
    endStop2 = digitalRead(Endstop2_Pin); // Check the endstop switch
  }
}

void calibrate()
{
  Serial.println("Entering calibration mode...");

  calibrationMode = true; // Set calibration flag

  digitalWrite(LED_Status1, HIGH); // Calibration mode LED ON

  while (calibrationMode == true)
  {
    int button1 = digitalRead(Btn1_Pin);              // Read blind 1 open button
    int button2 = digitalRead(Btn2_Pin);              // Read blind 1 close button
    int button3 = digitalRead(Btn3_Pin);              // Read blind 2 open button
    int button4 = digitalRead(Btn4_Pin);              // Read blind 2 close button
    int CalibrateBtn = digitalRead(CalibrateBtn_Pin); // Read calibrate button

    if (button1 == LOW) // If blind 1 open button pressed
    {
      Serial.println("Open blind 1 button pressed...");
      delay(500);
      stepopen(1);
    }
    if (button2 == LOW) // If blind 1 close button pressed
    {
      Serial.println("Close blind 1 button pressed...");
      delay(500);
      stepclosed(1, 4);
    }
    if (button3 == LOW) // If blind 2 open button pressed
    {
      Serial.println("Open blind 2 button pressed...");
      delay(500);
      stepopen(2);
    }
    if (button4 == LOW) // If blind 2 close button pressed
    {
      Serial.println("Close blind 2 button pressed...");
      delay(500);
      stepclosed(2, 4);
    }
    if (CalibrateBtn == LOW) // If calibration button pressed
    {
      Serial.println("Calibrate button pressed...");
      calibrationMode = false;        // Set calibration flag
      digitalWrite(LED_Status1, LOW); // Calibration mode LED ON
      Serial.println("Leaving calibration mode...");
      delay(500);
    }
  }
}

void buttonpolling()
{
  int button1 = digitalRead(Btn1_Pin);              // Read blind 1 open button
  int button2 = digitalRead(Btn2_Pin);              // Read blind 1 close button
  int button3 = digitalRead(Btn3_Pin);              // Read blind 2 open button
  int button4 = digitalRead(Btn4_Pin);              // Read blind 2 close button
  int CalibrateBtn = digitalRead(CalibrateBtn_Pin); // Read calibrate button

  if (button1 == LOW) // Blind 1 open button pressed
  {
    Serial.println("Open blind 1 button pressed...");
    if (blind1Open == true) // Don't do anything if blinds are already open
    {
      Serial.println("Blind 1 already open...");
      Serial.println("Nothing to do");
    }
    if (blind1Open == false)
    {
      openBlinds(1); // Call open blinds for blind 1
    }
    delay(500);
  }
  if (button2 == LOW) // Blind 1 close button pressed
  {
    Serial.println("Close blind 1 button pressed...");
    if (blind1Open == false) // Don't do anything if blinds are already closed
    {
      Serial.println("Blind 1 already closed...");
      Serial.println("Nothing to do");
    }
    if (blind1Open == true)
    {
      closeBlinds(1); // Call close blinds for blind 1
    }
    delay(500);
  }
  if (button3 == LOW) // Blind 2 open button pressed
  {
    Serial.println("Open blind 2 button pressed...");
    if (blind2Open == false) // Don't do anything if blinds are already closed
    {
      Serial.println("Blind 2 already closed...");
      Serial.println("Nothing to do");
    }
    if (blind2Open == true)
    {
      openBlinds(2); // Call open blinds for blind 2
    }
    delay(500);
  }
  if (button4 == LOW) // Blind 2 close button pressed
  {
    Serial.println("Close blind 2 button pressed...");
    if (blind2Open == false) // Don't do anything if blinds are already closed
    {
      Serial.println("Blind 2 already closed...");
      Serial.println("Nothing to do");
    }
    if (blind2Open == true)
    {
      closeBlinds(2); // Call close blinds for blind 2
    }
    delay(500);
  }
  if (CalibrateBtn == LOW) // If calibration button is pushed
  {
    Serial.println("Calibrate button pressed...");
    delay(500);
    calibrate(); // Call calibration function
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  String response;

  for (int i = 0; i < length; i++)
  {
    response += (char)payload[i];
  }
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(response);

  if (strcmp(topic, TOPIC1) == 0)
  {
    if (response == "OPEN")
    {
      Serial.println("MQTT says blind 1 Open");
      if (blind1Open == true) // Don't do anything if blinds are already open
      {
        Serial.println("Blind 1 already open...");
        Serial.println("Nothing to do");
      }
      if (blind1Open == false) // But if blinds are closed, open them
      {
        openBlinds(1); // Call open blinds for blind 1
      }
    }
    else if (response == "CLOSED")
    {
      Serial.println("MQTT says blind 1 Closed");
      if (blind1Open == false) // Don't do anything if blinds are already closed
      {
        Serial.println("Blind 1 already closed...");
        Serial.println("Nothing to do");
      }
      if (blind1Open == true) // But if blinds are opened, close them
      {
        closeBlinds(1); // Call close blinds for blind 1
      }
    }
    else if (response == "STOP")
    {
      Serial.println("MQTT says blinds Stop");
      motorStop();
    }
    else if (response == "OVERRIDE_CLOSE")
    {
      Serial.println("MQTT Close with Override");
      stepclosed(1, 1); // Step close motor 1, 100 steps (multiplier 1)
    }
  }
  if (strcmp(topic, TOPIC2) == 0)
  {
    if (response == "OPEN")
    {
      Serial.println("MQTT says blind 2 Open");
      if (blind2Open == true) // Don't do anything if blinds are already open
      {
        Serial.println("Blind 2 already Open...");
        Serial.println("Nothing to do");
      }
      if (blind2Open == false) // But if blinds are closed, open them
      {
        openBlinds(2); // Call open blinds for blind 2
      }
    }
    else if (response == "CLOSED")
    {
      Serial.println("MQTT says blind 2 Close");
      if (blind2Open == false) // Don't do anything if blinds are already closed
      {
        Serial.println("Blind 2 already closed...");
        Serial.println("Nothing to do");
      }
      if (blind2Open == true) // But if blinds are opened, close them
      {
        closeBlinds(2); // Call close blinds for blind 2
      }
    }
    else if (response == "STOP")
    {
      Serial.println("MQTT says blinds Stop");
      motorStop();
    }
    else if (response == "OVERRIDE_CLOSE")
    {
      Serial.println("MQTT Close with Override");
      stepclosed(2, 1); // Step close motor 2, 100 steps (multiplier 1)
    }
  }
}

// Connect to WiFi network
void setup_wifi()
{
  Serial.print("\nConnecting to ");
  Serial.println(ssid);

  WiFi.setHostname(ID);       // Set hostname
  WiFi.begin(ssid, password); // Connect to network

  while (WiFi.status() != WL_CONNECTED)
  { // Wait for connection
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("Hostname: ");
  Serial.println(ID);

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{ // Reconnect to client
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(ID))
    {
      client.subscribe(TOPIC1);
      Serial.println("connected");
      Serial.print("Subcribed to: ");
      Serial.println(TOPIC1);
      Serial.println('\n');

      client.subscribe(TOPIC2);
      Serial.println("connected");
      Serial.print("Subcribed to: ");
      Serial.println(TOPIC2);
      Serial.println('\n');
    }
    else
    {
      Serial.println(" try again in 5 seconds"); // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200); // Set-up serial port for debugging

  setup_wifi(); // Connect to network

  stepper1.setSpeed(stepperSpeed); // Set stepper speed
  stepper2.setSpeed(stepperSpeed); // Set stepper speed

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_Status1, OUTPUT);
  pinMode(Btn1_Pin, INPUT);                // Blind 1 open button
  pinMode(Btn2_Pin, INPUT);                // Blind 1 close button
  pinMode(Btn3_Pin, INPUT_PULLUP);         // Blind 2 open button as input with pullup
  pinMode(Btn4_Pin, INPUT_PULLUP);         // Blind 2 close button as input with pullup
  pinMode(CalibrateBtn_Pin, INPUT_PULLUP); // Calibrate button as input with pullup

  startupCalibration(); // Run startup calibration to make sure blinds are in known state

  client.setServer(broker, 1883);
  client.setCallback(callback); // Initialize the callback routine
}

void loop()
{
  if (!client.connected()) // Reconnect if connection is lost
  {
    reconnect();
  }
  client.loop();
  buttonpolling();
}