// transmitter.pde
//
// Simple example of how to use VirtualWire to transmit messages
// Implements a simplex (one-way) transmitter with an TX-C1 module
//
// See VirtualWire.h for detailed API docs
// Author: Mike McCauley (mikem@airspayce.com)
// Copyright (C) 2008 Mike McCauley
// $Id: transmitter.pde,v 1.3 2009/03/30 00:07:24 mikem Exp $

#include <VirtualWire.h>

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

const int led_pin = 13;
const int transmit_pin = 12;
const int receive_pin = 2;
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
  Serial.begin(9600);  // Debugging only
  Serial.println("setup");

  // LCD
  display.begin();

  // Initialise the IO and ISR
  vw_set_tx_pin(transmit_pin);
  vw_set_rx_pin(receive_pin);
  vw_set_ptt_pin(transmit_en_pin);
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(bit_per_second);  // Bits per sec

  vw_rx_start();       // Start the receiver PLL running

  pinMode(led_pin, OUTPUT);

  /* Set the last time we recieved a ping to now */
  last_time = micros();
  amount_success = 10;
}

void loop()
{
  // 1. Send message
  sendMessage();

  // 2. Update display
  drawDisplay();
}

/* Do all the display drawing here */
void drawDisplay() 
{
  display.clearDisplay();   // clears the screen and buffer
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  display.print("T: ");
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

void sendMessage() {
  char msg[7] = {'h','e','l','l','o',' ','#'};

  msg[msg_counter_pos] = counter;
  digitalWrite(led_pin, HIGH); // Flash a light to show transmitting
  vw_send((uint8_t *)msg, 7);
  vw_wait_tx(); // Wait until the whole message is gone
  last_time = micros();
  digitalWrite(led_pin, LOW);
  //delayMicroseconds(interval_micros);
  delay(interval_micros/1000); 
  counter++;
}

