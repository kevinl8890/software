# software
Firmware for the ultrasound modem

First untested version of the combined sender/reciever firmware.
Uses RTC based timing synchronisation.
Also uses the RadioHead library instead of the obsolete VirtualWire library.

buddy_modem contains the latest combined firmware

#Old firmware for separate sender/reciever:

- recieverj is the standalone receiver, wired for display on a Nokia LCD
- transmitter is the standalone transmitter

Both use the VirtualWire library (superseded by the RadioHead library)
