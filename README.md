#Smog Printer - Arduino

This is the Arduino code for the Smog Printer. Connects to the [Smog Printer web service](https://github.com/markchang/smogprinter-rails).

Some development notes:

  * Serial console output should not be used at the same time as the thermal printer. I don't know if this is a global issue, but it is for us! So, that's why there are PRINT #ifdefs everywhere.
  * printer.begin() causes some linefeeds. Deal with it.
  * The Ethernet library seems to eat any MAC that you give it. I don't know if on the newest Ethernet Shield this programs the MAC address or not. Either way, it causes much havoc. Use your real MAC address until we figure this out.