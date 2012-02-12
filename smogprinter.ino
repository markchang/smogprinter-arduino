/**
 * SmogPrinter
 * Arduino-based "cloud" printer, ala Berg Printer
 **/

#include "Thermal.h"
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Ethernet.h>
#include <WebServer.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>

#define NAMELEN 32
#define API_KEY_LEN 40
#define API_KEY_VALID 0
#define API_KEY 1
#define PRINT 1

// printer configs
const int printer_RX_Pin = 2;  // this is the green wire
const int printer_TX_Pin = 3;  // this is the yellow wire

// track connection state
boolean client_started = false;

// Ethernet library requires MAC address of the shield
byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x97, 0x7B };
// byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

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
#ifdef PRINT
	Thermal printer(printer_RX_Pin, printer_TX_Pin);
#endif
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
  server.httpSuccess(); // send 200
  if( type != WebServer::HEAD ) {
    server.printP(page_header);
    
    if(api_key_valid()) {
      // read API key and display
      read_api_key(api_key);
      server.printP(api_key_set);
      for(int i=0; i<API_KEY_LEN; i++) {
        server.print(api_key[i]);
      }
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
  
  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type == WebServer::HEAD)
    return;

  if (strlen(url_tail)) {
    while (strlen(url_tail)) {
      rc = server.nextURLparam(&url_tail, name, NAMELEN, value, API_KEY_LEN + 1);
      if (rc != URLPARAM_EOS) {
        write_api_key(value);
				#ifndef PRINT
        for(int i=0; i< API_KEY_LEN; i++) {
          Serial.print(value[i]);
        }
        Serial.println();
				#endif
      }
    }

    server.httpSeeOther("/");
  }
}

void web_destroy_api_key(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
  destroy_api_key();
  server.httpSeeOther("/");
}

void connectToServer() {
  if(client.connect("smog.heroku.com", 80)) {
    client.println("GET /feeds?api_key=bb20519cb1dbb84e4c393f579702321893f39b9d HTTP/1.1");
    client.println("HOST: smog.heroku.com:80");
    client.println();
    client_started = true;
  }
}

void setup() {
  // reserve space
  currentLine.reserve(256);
  tweet.reserve(150);

  // initialize printer and serial
  Serial.begin(9600);

  // pause so I can turn on serial monitoring
	#ifndef PRINT
  Serial.println("Smog Printer awake.");
	Serial.println("... 3 seconds ...");
	#endif
	delay(3000);

  // start the Ethernet connection
  if (Ethernet.begin(mac) == 0) {
		#ifndef PRINT
    Serial.println("Failed to configure Ethernet using DHCP");
		#endif
    // no point in carrying on, so do nothing forevermore:
    for(;;)
			;
  }

  // we have a DHCP license and IP address, continue setup
  // log the acquired IP address to the serial monitor
	#ifndef PRINT
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
	#endif

  // welcome user
	#ifdef PRINT
		printer.begin();
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
	#endif
  
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
          #ifdef PRINT
 						printer.println(tweet);
 	          printer.feed(3);
					#else
	          Serial.println(tweet);
 					#endif
          client.stop();
          client_started = false;
        }
      }
    } 
  } else if(!client.connected() && client_started) {
    // close connection
		#ifndef PRINT
    Serial.println();
    Serial.println("disconnecting.");
		#endif
    client.stop();
    client_started = false;
  }
}
