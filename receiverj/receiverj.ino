// receiver.pde
//
// Simple example of how to use VirtualWire to receive messages
// Implements a simplex (one-way) receiver with an Rx-B1 module
//
// See VirtualWire.h for detailed API docs
// Author: Mike McCauley (mikem@airspayce.com)
// Copyright (C) 2008 Mike McCauley
// $Id: receiver.pde,v 1.3 2009/03/30 00:07:24 mikem Exp $

#include <VirtualWire.h>

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

const int led_pin = 13;      
const int transmit_pin = 12;
const int receive_pin = 11;
const int transmit_en_pin = 3;
const int bit_per_second = 200;

// Software SPI (slower updates, more flexible pin options):
// pin 7 - Serial clock out (SCLK)
// pin 6 - Serial data out (DIN)
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(7, 6, 5, 4, 3);

/* Internal program variables */
unsigned long last_time;
byte          counter               = 1;
int           amount_success;

// Constants
const unsigned long interval_micros = 500000; // 500ms is 500.000 us 
const int           msg_counter_pos = 6;

void setup()
{
  // 0. Setup

  //
  delay(1000);
  Serial.begin(9600);	// Debugging only
  Serial.println("setup");

  // LCD
  display.begin();

  // Initialise the IO and ISR
  vw_set_tx_pin(transmit_pin);
  vw_set_rx_pin(receive_pin);
  vw_set_ptt_pin(transmit_en_pin);
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(bit_per_second);	 // Bits per sec

  vw_rx_start();       // Start the receiver PLL running

  pinMode(led_pin, OUTPUT);

  /* Set the last time we recieved a ping to now */
  last_time = micros();
  amount_success = 0;
    
}

/* The main loop */
void loop()
{
  // 1. Try to recieve a message
  doRecieve();
  
  // 2. Update the display
  drawDisplay();
}

/* Try to recieve a message */
void doRecieve() 
{
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;

  if (vw_get_message(buf, &buflen)) // Non-blocking
  {
    int i;
    digitalWrite(led_pin, HIGH); // Flash a light to show received good message

    processMessage(buf, &buflen);

    digitalWrite(led_pin, LOW);
  }  
}

/* Do all the display drawing here */
void drawDisplay() 
{
  display.clearDisplay();   // clears the screen and buffer
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  display.print("R: ");
  display.setTextColor(WHITE, BLACK); // 'inverted' text

  for (int i = 0; i <= amount_success; i++) {
     display.print(" ");
  }

  display.setTextColor(BLACK); 
  display.println("%");

  display.print((amount_success/10)*100); display.println(" %");

  unsigned long diff = micros() -last_time;
  
  display.print("D: "); display.println(diff);
  display.print("C: "); display.println(counter);
    
  // And draw everything to the screen
  display.display();
}

/* Process a recieved message */
void processMessage(uint8_t* buf, uint8_t* buflen)
{
  unsigned long recieved = micros();
  
  // How long did it take to recieve this message?
  unsigned long travel_time = recieved - last_time;

  // Reset the last time to now
  last_time = recieved;

  // Read the counter
  byte recieved_counter = buf[msg_counter_pos];

  // increment our local counter
  counter++;

  // Compare the counters
  if (counter == recieved_counter) 
  {
    amount_success++;
  }
  else 
  {
    amount_success--;
  }

  // Keep it between 0 and 10
  amount_success = constrain(amount_success, 0, 10);

  // set our local counter to the recieved remote one
  counter = recieved_counter;
}



