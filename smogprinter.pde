/**
 * SmogPrinter
 * Arduino-based "cloud" printer, ala Berg Printer
 **/

#include "SoftwareSerial.h"
#include "Thermal.h"
#include <SPI.h>
#include <Ethernet.h>

int printer_RX_Pin = 2;  // this is the green wire
int printer_TX_Pin = 3;  // this is the yellow wire

Thermal printer(printer_RX_Pin, printer_TX_Pin);

// Ethernet library requires MAC address of the shield
byte mac[] = {  
  0x90, 0xA2, 0xDA, 0x00, 0x97, 0x7B };

EthernetClient client;

void setup(){
  Serial.begin(9600);
  
  // start the Ethernet connection
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }

	// now we have a DHCP license and the stack is initialized

  // log the acquired IP address
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

  // print the acquired IP address
  printer.begin();
  printer.feed(1);
  printer.justify('L'); 
  printer.setSize('M');
  printer.println("My IP Address:");
  printer.println(Ethernet.localIP());
  delay(3000);

  printer.feed(3);
}

// main loop
void loop(){
  // actually do nothing  
}
