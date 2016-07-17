/**
  buddyAssist modem
  Name: buddy_modem
  Purpose: Ultrasound underwater communication platform.

  @author Jelle Tigchelaar
  @version 0.1 19-06-2016

  The buddyAssist modem is part of the buddyAssist project by scubAssist.
  The modem is used for ultrasonic communication adn distance measurements under water.
  For more information regarding the project visit: http://www.scubassist.com/
  All sources are maintained at: https://github.com/scubAssist

  Code and hardware are released under CC BY-SA 4.0
  http://creativecommons.org/licenses/by-sa/4.0/
*/

// D11 TX modem
// D12 RX modem
// A4 SDA i2c RTC
// A5 SCL i2c RTC
// D2 interrupt RTC
// D3 interrupt RF

#include <RH_ASK.h>
#include <SPI.h> // Not actualy used but needed to compile
#include <DS3232RTC.h>
#include <Time.h>
#include <Wire.h>

const int modem_tx_pin = 11;
const int modem_rx_pin = 12;
const int modem_ptt_pin = 13;
const int modem_baud_rate = 200;
const int sda_pin = 4;
const int scl_pin = 5;
const int rtc_interrupt_pin = 2;
const int rf_interrupt_pin = 3;
const int rf_transmit_pin = 10;

//RH_ASK driver;
RH_ASK driver(modem_baud_rate, modem_tx_pin, modem_rx_pin, modem_ptt_pin);

struct payload_diver {
  byte id     : 6; //6 bit duiker id 0..63
  byte x      : 7; //7 bit x grid (0..127, bereik van ongeveer 500 meter)
  byte y      : 7; //7 bit y grid (0..127, bereik van ongeveer 500 meter)
  byte depth  : 7; //7 bit depth in meters 0..127
  byte oxygen : 6; //6 bit oxygen in steps of 10 bar 0..63
  byte status : 8; //8 bit status flags (bitmask)
};

struct payload_buoy {
  byte id     : 8; //8 bit buoy id 0..255
};

typedef struct payload_diver PayloadDiver;
typedef struct payload_buoy PayloadBuoy;

const byte status_decompression = 0x01; // Decompression limit exceeded
const byte status_oxygen        = 0x02; // Oxygen low
const byte status_medical       = 0x04; // Medical attention required

volatile long zero_milliseconds = 0;
volatile bool rf_synched = false;

// Which mode do we operate in
bool is_diver = false;
bool is_buoy = false;
bool is_master = false;

// Diver variables
PayloadDiver *diverstatus;
bool is_under_water = false;

// Buoy variables
PayloadBuoy   *buoy;
volatile int  frameCounter = 0;
volatile int  zero_pulse_reset = 0;

PayloadBuoy   *buoy_recv;
PayloadDiver  *diver_recv;

// Serial command variables
#define INPUT_SIZE 30

void setup()
{
  Serial.begin(115760);	// Debugging only

  if (!driver.init())
    Serial.println("init failed");

  initializeDiver();
  initializeBuoy();

  // Init pins
  pinMode(rf_transmit_pin, OUTPUT);
  pinMode(rtc_interrupt_pin, INPUT_PULLUP);
  pinMode(rf_interrupt_pin, INPUT_PULLUP);

  // reset zero milliseconds
  zero_milliseconds = millis();

  // Interrupts aanzetten
  attachInterrupt(digitalPinToInterrupt(rtc_interrupt_pin), rtc_interrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(rf_interrupt_pin), rf_interrupt, RISING);
}

void activateRTCAlarm()
{
  RTC.setAlarm(ALM1_EVERY_SECOND, 1, 0, 0, 0);
  RTC.alarmInterrupt(ALARM_1, true);
}

void deactivateRTCAlarm()
{
  RTC.alarmInterrupt(ALARM_1, false);
}

void rtc_interrupt()
{
  if (is_diver) {
    zero_milliseconds = millis();

    // Random status send interval
    //send at random interval
    driver.send((uint8_t *)diverstatus, sizeof(PayloadDiver));
    driver.waitPacketSent();
  }

  if (is_buoy)
  {
    if (is_master)
    {
      // Send RF pulse every specified interval
      if (frameCounter == 0)
      {
        digitalWrite(rf_transmit_pin, HIGH);
        delayMicroseconds(1000); // Should work in ISR
        digitalWrite(rf_transmit_pin, LOW);
      }
    }

    // Send pulse when our ID equals the zero pulse + ID * seconds
    if (frameCounter == buoy->id)
    {
      driver.send((uint8_t *)buoy, sizeof(PayloadBuoy));
      driver.waitPacketSent();
    }
  }

  // Always increment the framecounter
  frameCounter++; 

  // if framecounter reaches max, reset to 0 (assume 16 frames, 0-15 = 16 frames, reset at 16)
  if (frameCounter == zero_pulse_reset)
  {
    frameCounter = 0;
  }
}

void rf_interrupt()
{
  // update rtc with zero seconds
  setTime(17, 6, 0, 11, 4, 2016); // not-so-arbitrary date
  rf_synched = true;
  frameCounter = 0; // Only occurs on zero reset, so reset the frame counter as well
}

void initializeDiver()
{
  diverstatus->id = 0;
  diverstatus->x = 1;
  diverstatus->y = 1;
  diverstatus->depth = 0;
  diverstatus->oxygen = 32;
  diverstatus->status = 0xb0;

  // Set the medical attention flag true
  diverstatus->status = diverstatus->status & status_medical;
}

void initializeBuoy()
{
  buoy->id = 0;
}

void serialWriteBuoy(PayloadBuoy *buoydata, long timediff)
{
  Serial.print(buoydata->id, DEC);
  Serial.print(",");
  Serial.println(timediff, DEC);
}

void serialWriteDiver(PayloadDiver *diverdata)
{
  Serial.print(diverdata->id, DEC);
  Serial.print(",");
  Serial.print(diverstatus->x, DEC);
  Serial.print(",");
  Serial.print(diverstatus->y, DEC);
  Serial.print(",");
  Serial.print(diverstatus->depth, DEC);
  Serial.print(",");
  Serial.print(diverstatus->oxygen, DEC);
  Serial.print(",");
  Serial.println(diverstatus->status, BIN);
}

void processSerialCommands()
{
  char input[INPUT_SIZE + 1];
  byte size = Serial.readBytes(input, INPUT_SIZE);
  // Add the final 0 to end the C string
  input[size] = 0;

  // Read each command pair
  char* command = strtok(input, "&");
  while (command != 0)
  {
    // Split the command in two values
    char* separator = strchr(command, ':');
    if (separator != 0)
    {
      *separator = 0;
      int cmdId = atoi(command);
      ++separator;
      int parameter = atoi(separator);

      switch (cmdId)
      {
        case 0:    // Diver or buoy?
          if (parameter == 0)
          {
            is_diver = true;
            is_buoy = false;
          }
          if (parameter == 1)
          {
            is_diver = false;
            is_buoy = true;
          }

          Serial.println("is_diver: " + is_diver);
          Serial.println("is_buoy: " + is_buoy);
          break;
        case 1:   // Time zero for master
          if (is_buoy)
          {
            is_master = true;
          }
          Serial.println("is_master: " + is_master);
          break;
        case 2:   // Set id for frame_interval
          if (is_buoy)
          {
            buoy->id = parameter;
          }
          break;
        case 3:   // Set id for diver
          if (is_diver)
          {
            diverstatus->id = parameter;
          }
          break;
        case 4:    // Total amount of buoys
          zero_pulse_reset = parameter;
          break;
        case 5:   // Are we under water
          if (is_diver)
          {
            if (parameter == 1)
            {
              is_under_water = true;
              activateRTCAlarm();
            }
            else
            {
              is_under_water = false;
              deactivateRTCAlarm();
            }
          }
          break;
      }
    }

    // Find the next command in input string
    command = strtok(0, "&");
  }
}

void loop()
{
  // recieve potential UX messages
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen)) // Non-blocking
  {
    if (is_diver)
    {
      // If diver we need to check the time diff FIRST
      long timediff = millis() - zero_milliseconds;

      // Diver recieves buoy pings
      buoy_recv = (PayloadBuoy *)buf;
      serialWriteBuoy(buoy_recv, timediff);
    }

    if (is_buoy)
    {
      // Buoy recieves diver updates
      // buffer to struct
      diver_recv = (PayloadDiver *)buf;
      serialWriteDiver(diver_recv);
    }

    // Message with a good checksum received, dump it.
    driver.printBuffer("Got:", buf, buflen);
  }

  // Check serial for input
  if (Serial.available() > 0)
  {
    // Process the serial commands
    processSerialCommands();
  }
}

