# eGarageOpener
Software for triggering a relay for a number of milli-seconds

See 'Blog Henri Matthijssen' for instructions for the hardware and setup
http://matthijsseninfo.nl

(c) 2019 Henri Matthijssen (henri@matthijsseninfo.nl)

Please do not distribute without permission of the original author and 
respect his time and work spent on this.

First time you installed the software the ESP8266 will startup in Access-Point (AP) mode.
Connect with you WiFi to this AP using next information:

AP ssid           : eGarageOpener
AP password       : eGarageOpener

Enter IP-address 192.168.4.1 in your web-browser. This will present you with dialog to fill
the credentials of your home WiFi Network. Fill your home SSID and password and press the
submit button. Your ESP8266 will reboot and connect automatically to your home WiFi Network.
Now find the assigned IP-address to the ESP8266 and use that again in your web-browser.

Before you can use the Web-Interface you have to login with a valid user-name
and password on location: http://ip-address/login (cookie is valid for 24 hours).

Default user-name : admin
Default password  : notdodo

In the Web-Interface you can change the default user-name / password (among other
settings like pulse-time).

When using the API you always need to supply a valid value for the API key
(default value=27031969). The API has next format in the URL:

http://ip-address/api?action=xxx&value=yyy&api=zzz

Currently next API actions are supported:

relay, value					(value=pulse-time in ms)

You can upgrade the firmware with the (hidden) firmware upgrade URL:
http://ip-address/upgradefw
(you first needed to be logged in for the above URL to work)

You can erase all settings by clearing the EEPROM with next URL:
http://ip-address/erase
(you first needed to be logged in for the above URL to work)
