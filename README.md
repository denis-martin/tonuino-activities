# TonUINO Activities

This is an alternative firmware for the [TonUINO](https://www.voss.earth/tonuino/), an Arduino-based music player using NFC/RFID cards for selecting MP3 files to be played from an SD card.

Compared to the [original firmware](https://github.com/xfjx/TonUINO), this alternative firmware:
* starts up faster
* has a cleaner code structure for easy extensibility
* introduces the concept of "activities": any music game can be added by implementing the activity class
* supports both, the [Arduino IDE](https://www.arduino.cc) as well as [Platform IO](https://platformio.org/)
* allows to disable serial debug output which massively reduces the needed flash space (i.e., it would also fit into a 16k microcontroller)

The MP3 file IDs for menus/adverts are choosen so that they do not conflict with the original firmware. The data saved on the NFC/RFID tags and in the EEPROM is not compatible with the original firmware. However, such a compatibility could be added if there is enough interest.

## Arduino IDE

You need to install the following libraries via the library manager:
* MFRC522
* DFPlayer Mini Mp3 by Makuna
* JC_Button

You also need to configure the Board (Arduino Nano) and the Prozessor (Arduino328P (Old Bootloader)) in the Arduino IDE.

Navigate to the "TonUINO-Activities" subfolder of this repository and open the file "TonUINO-Activities.ino".

## Platform IO

Open this repository in VSCode with Platform IO installed and hit compile ;)
