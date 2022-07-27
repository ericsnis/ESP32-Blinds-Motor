# ESP32 Blinds Motor
Blind motor automation using MQTT & ESP32

## Features

  It connects to an MQTT server and:
  - checks washing blinds open/closed status (topic blinds/nbrblind1)
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
      - Calibration on pin 5
      - Up button on pin 39
      - Down button on pin 36
    Amount of backlash in the blinds rod can be adjusted

  Stepper driver info
  - Uses ULN2003 based board KS0327 Keyestudio stepper motor drive board
  - The stepper driver is attached to digital pins 9, 13, 12 & 14 of the ESP32.
    - Pins defined as Stepper1In1, Stepper1In2, Stepper1In3, etc.
    - Connect in the order they appear on board i.e. In1, In2, In3, In4
  - Driver board should be connected to 5V source

  Assumes use of ESP32 based NodeMCU board, not similarly named ESP8266 NodeMCU.
  - Can be changed to other boards if pinout is changed

  Uses the following libraries
  - Arduino stepper library released under GNU Lesser General Public
  - PubSubClient by Nick O'Leary released under MIT license

## Boards
The schematic folder contains the board and schematic files in Eagle format
- Designs use 1206 SMT LEDs and resistors
- One design excludes headers for endstop microswitches
- One design includes a header for a endstop microswitch

- ESP32 Blinds Motor
![ESP32 Blinds Motor Board without Endstops](/assets/images/BlindsMotorBoard.png)

- ESP32 Blinds Motor w/ Endstop Header
![ESP32 Blinds Motor Board with Endstops](/assets/images/BlindsMotorBoardEndstops.png)

## Code
The code folder contains the program code to compile and upload to the ESP32
- Use [Platform.io](https://docs.platformio.org/en/latest/integration/ide/vscode.html) installed on [Visual Studio Code](https://code.visualstudio.com/) AKA: VSCode to compile and upload.
- Open the code folder in VSCode then compile, PlatformIO should download any dependencies.
- Project Environments
  - esp32dev-steppermqtt no endstops - Firmware without endstops enabled
  - esp32dev-stepper mqtt with endstop - Firmware with endstops enabled
  - esp32dev-stepper test - Stepper motor connection test