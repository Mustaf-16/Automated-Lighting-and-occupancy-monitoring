# Lecture Room Occupancy Controller

This ESP32 project uses a PIR motion sensor to detect whether a lecture room
is occupied. When motion is detected, the controller turns on the room light
and the occupancy indicator. The light turns off after five minutes without
motion.

The project is intended to be run in the **Wokwi Simulator extension for
Visual Studio Code** with the **ESP-IDF framework**. No physical ESP32 board
or device flashing is required.

## Features

- Detects motion with a PIR sensor.
- Turns the room light on when motion is detected.
- Turns the light off after five minutes without motion.
- Shows occupancy with a green LED.
- Reports state changes and periodic status messages in the serial monitor.

## Requirements

Install the following in Visual Studio Code:

- Espressif IDF extension
- Wokwi for VS Code extension
- ESP-IDF and its required tools

The Wokwi extension may require a Wokwi account or token, depending on your
setup.

## Run the simulation in VS Code

1. Open this project folder in Visual Studio Code.
2. Select the ESP-IDF target using the Espressif IDF extension if prompted.
3. Build the project using the ESP-IDF extension:
   - Open the Command Palette with `Ctrl+Shift+P`.
   - Run **ESP-IDF: Build your project**.
4. Start Wokwi:
   - Open [`diagram.json`](./diagram.json).
   - Click the Wokwi **Start Simulation** button, or run
     **Wokwi: Start Simulator** from the Command Palette.
5. Use the Wokwi serial monitor to view the controller logs.

The firmware and ELF paths used by Wokwi are configured in
[`wokwi.toml`](./wokwi.toml). The project must be built before starting the
simulation so that `build/SmartRoom.bin` and `build/SmartRoom.elf` exist.

## Test the simulation

In the Wokwi simulator, activate the PIR sensor to simulate motion. The
following outputs should respond:

| Component | ESP32 pin | Expected behavior |
| --- | --- | --- |
| PIR sensor output | GPIO 27 | Detects motion |
| Relay control | GPIO 26 | Controls the room-light circuit |
| Occupancy LED | GPIO 19 | On while the room is occupied |

The yellow LED represents the room light and the green LED represents room
occupancy. After the last motion event, the controller reports the room as
vacant and turns the outputs off when the five-minute timeout expires.

## Serial monitor messages

The serial monitor reports messages such as:

```text
Motion detected: room OCCUPIED
Light -> ON
STATUS: occupied | light ON | vacant in 295 s | events 1
```

## Project structure

```text
.
├── main/              ESP-IDF application source code
├── diagram.json       Wokwi circuit definition
├── wokwi.toml         Wokwi firmware and ELF paths
├── sdkconfig          ESP-IDF project configuration
└── CMakeLists.txt     ESP-IDF build configuration
```

## Troubleshooting

- **Wokwi cannot find the firmware:** run **ESP-IDF: Build your project**
  first and confirm that the `build` folder contains the generated firmware.
- **The circuit is not visible:** open `diagram.json` and start the Wokwi
  simulator from that file.
- **No log output is visible:** open the Wokwi serial monitor and restart the
  simulation after rebuilding the project.