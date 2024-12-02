# RFID-Enabled Power Control with Arduino Uno
Arduino code that enables the power of a USB connection for a certain timespan after scanning a valid RFID tag. By powering tue USB connection of a master/slave socket strip, this code can be used to enable any kind of electrical device, not only devices with USB connection. Designed for high reliability and long running times.

The valid RFID tags are stored on a connected SD card and can be changed over the serial connection (potentially a bluetooth connection) or by simply editing the files on the SD card and restarting the Arduino with the new files. Events are stored on a log file on the same SD card.

## Setup
The following modules are used in this setup:
- Arudino Uno
- RFID reader
- Master/slave socket strip with USB connection to enable the slaves
- SD card module with SD card
- A real time module

### Port setup:
TODO

## Commands
TODO

## Known Issues
The power output is supposed to be enabled for a specific timespan (44 seconds). This is not the exact value at the moment because the logging takes an indefinite amount of time. There is a currently disabled solution in the code that has to be tested first.

Witing in the log files may lead to a corrupted log file and all following logs can not be saved anymore. A simple workaround with new log files will be implemented.