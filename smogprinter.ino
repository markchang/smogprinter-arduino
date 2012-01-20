/**
 * SmogPrinter
 * Arduino-based "cloud" printer, ala Berg Printer
 **/

#include "SoftwareSerial.h"
#include "Thermal.h"
#include <SPI.h>
#include <Ethernet.h>
#include <WebServer.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>

#define NAMELEN 32
#define API_KEY_LEN 40
#define API_KEY_VALID 0
#define API_KEY 1

// printer configs
const int printer_RX_Pin = 2;  // this is the green wire
const int printer_TX_Pin = 3;  // this is the yellow wire

// track connection state
boolean client_started = false;

// Ethernet library requires MAC address of the shield
byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x97, 0x7B };

// API key holder
char api_key[API_KEY_LEN];

// Twitter parser
String currentLine = "";
String tweet = "";
boolean readingTweet = false;

// ROM-based strings
P(page_header) = "<html><head><title>Smog Printer</title></head><body>";
P(api_key_form) = "<p><form action=\"setkey.html\" method=\"get\">API KEY: <input type=\"text\" name=\"api_key\" size=\"40\" /><input type=\"submit\" value=\"Submit\" /></form>";
P(api_key_destroy_form) = "<form action=\"destroykey.html\" method=\"get\"><input type=\"submit\" value=\"Destroy Key\" /></form></p>";
P(page_footer) = "</body></html>";
P(api_key_set) = "API Key set to: ";
P(api_key_invalid) = "No API key is set";
P(separator) = " : ";

// global instances
Thermal printer(printer_RX_Pin, printer_TX_Pin);
EthernetClient client;
WebServer webserver("", 80);

void write_api_key(char *api_key)
{
	// set valid bit
	EEPROM.write(API_KEY_VALID, 1);
	
	// write api_key (length 40)
	for(int i=0; i<API_KEY_LEN; i++) {
		EEPROM.write(API_KEY + i, (byte)api_key[i]);
	}
}

void destroy_api_key()
{
	// unset the valid bit
	EEPROM.write(API_KEY_VALID, 0);
}

void read_api_key(char *api_key)
{
	for(int i=0; i<API_KEY_LEN; i++) {
		api_key[i] = (char)EEPROM.read(API_KEY + i);
	}
}

boolean api_key_valid()
{
	byte valid_bit = EEPROM.read(API_KEY_VALID);
	
	if( valid_bit == 1 ) return true;
	else return false;
}

void web_index(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
	Serial.println("[index]");
	
	server.httpSuccess(); // send 200
	if( type != WebServer::HEAD ) {
		server.printP(page_header);
		
		if(api_key_valid()) {
			// read API key and display
			read_api_key(api_key);
			Serial.print("API Key set to: ");
			server.printP(api_key_set);
			for(int i=0; i<API_KEY_LEN; i++) {
				Serial.print(api_key[i]);
				server.print(api_key[i]);
			}
			Serial.println();
			server.println();
		} else {
			// note that we have no API key
			server.printP(api_key_invalid);
		}
		
		server.printP(api_key_form);
		server.printP(api_key_destroy_form);
		
		server.printP(page_footer);
	}
}

void web_set_api_key(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  URLPARAM_RESULT rc;
  char name[NAMELEN];
  int  name_len;
  char value[API_KEY_LEN];
  int value_len;

	Serial.println("[set api key]");
	
  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type == WebServer::HEAD)
    return;

  if (strlen(url_tail)) {
		Serial.print("Setting API key to: ");

	  while (strlen(url_tail)) {
	    rc = server.nextURLparam(&url_tail, name, NAMELEN, value, API_KEY_LEN + 1);
	    if (rc != URLPARAM_EOS) {
				write_api_key(value);
				for(int i=0; i< API_KEY_LEN; i++) {
					Serial.print(value[i]);
				}
				Serial.println();
			}
    }

		server.httpSeeOther("/");
  }
}

void web_destroy_api_key(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
	Serial.println("[destroy key]");
	
	destroy_api_key();
	server.httpSeeOther("/");
}

void connectToServer() {
	Serial.println("Connecting to server");
	
	if(client.connect("10.41.8.31", 3000)) {
		Serial.println("making HTTP request...");
		client.println("GET /feeds?api_key=221cd34a8d2291343e129151ca2239e9395dc6ae HTTP/1.1");
		client.println("HOST: 10.41.8.31:3000");
		client.println();
		client_started = true;
	}
}

void setup(){
  Serial.begin(9600);
	printer.begin();

	// reserve space
	currentLine.reserve(256);
	tweet.reserve(150);

	// pause so I can turn on serial monitoring
	delay(3000);
	Serial.println("Smog Printer awake.");

  // start the Ethernet connection
  if (Ethernet.begin(mac) == 0) {
    // Serial.println("Failed to configure Ethernet using DHCP");
		printer.println("Failed to get DHCP address. Bailing.");
		printer.feed(3);
		
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }

	// we have a DHCP license and IP address, continue setup
  // log the acquired IP address to the serial monitor
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

	// welcome user
  if(api_key_valid()) {
		printer.println("Smog Printer ready at ");
		printer.print("http://");
		printer.println(Ethernet.localIP());
	} else {
		printer.println("Smog Printer not configured.");
		printer.println("Visit me at");
		printer.print("http://");
		printer.println(Ethernet.localIP());
	}
	
  printer.feed(3);

	delay(3000);

	// now set up web server
	webserver.setDefaultCommand(&web_index);
	webserver.addCommand("setkey.html", &web_set_api_key);
	webserver.addCommand("destroykey.html", &web_destroy_api_key);
	
	// start the web server
	webserver.begin();
	
	connectToServer();
}

// main loop
void loop(){
	char buff[64];
	int len = 64;
	
	// process incoming connections one at a time forever
	webserver.processConnection(buff, &len);
	
	// process client connection
	if(client.connected()) {
		if(client.available()) {
			// read incoming byte
			char inChar = client.read();
			// add to end of line
			currentLine += inChar;
			// clear on newline
			if( inChar == '\n') {
				currentLine = "";
			}
			// if the currentLine ends (starts) with <tweet>, it will
			// be followed by the tweet
			if( currentLine.endsWith("<tweet>")) {
				// tweet is beginning, clear the current tweet string
				readingTweet = true;
				tweet = "";
			}
			// if we're in a tweet, add to the tweet string
			if( readingTweet ) {
				if(inChar != '<') {
					tweet += inChar;
				}
				else {
					// if we got a "<", we've reached the end of the tweet
					readingTweet = false;
					Serial.println(tweet);
					printer.println(tweet);
					printer.feed(3);
					client.stop();
					client_started = false;
				}
			}
		} 
	} else if(!client.connected() && client_started) {
		// close connection
		Serial.println();
		Serial.println("disconnecting.");
		client.stop();
		client_started = false;
	}
}
