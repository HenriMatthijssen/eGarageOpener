// =========================================================================================
// 
// eGarageOpener v1.01  Software for triggering a relay for a number of milli-seconds            
//
//                      See 'Blog Henri Matthijssen' for instructions for the hardware and setup
//                      http://matthijsseninfo.nl
//
//                      (c) 2019 Henri Matthijssen (henri@matthijsseninfo.nl)
//
//                      Please do not distribute without permission of the original author and 
//                      respect his time and work spent on this.
//
// =========================================================================================
//
// Please follow instructions below to compile this code with the Arduino IDE:
//
// Please make sure that you have set next line in 'Additional Board Manager URL's' of 
// Preferences:
//
// http://arduino.esp8266.com/stable/package_esp8266com_index.json
//
// Then goto Boards Manager (‘Tools > Board’) and install the ‘esp8266 by ESP8266 Community’ entry.
// Finally set your board to 'NodeMCU 1.0 (ESP-12E Module)' in Tools menu.
//
// =========================================================================================
// 
// First time you installed the software the ESP8266 will startup in Access-Point (AP) mode.
// Connect with you WiFi to this AP using next information:
// 
// AP ssid           : eGarageOpener
// AP password       : eGarageOpener
//
// Enter IP-address 192.168.4.1 in your web-browser. This will present you with dialog to fill
// the credentials of your home WiFi Network. Fill your home SSID and password and press the
// submit button. Your ESP8266 will reboot and connect automatically to your home WiFi Network.
// Now find the assigned IP-address to the ESP8266 and use that again in your web-browser.
//
// Before you can use the Web-Interface you have to login with a valid user-name
// and password on location: http://ip-address/login (cookie is valid for 24 hours).
//
// Default user-name : admin
// Default password  : notdodo
//
// In the Web-Interface you can change the default user-name / password (among other
// settings like pulse-time).
//
// When using the API you always need to supply a valid value for the API key
// (default value=27031969). The API has next format in the URL:
//
// http://ip-address/api?action=xxx&value=yyy&api=zzz
//
// Currently next API actions are supported:
//
// relay, value					(value=pulse-time in ms)
//
// You can upgrade the firmware with the (hidden) firmware upgrade URL:
// http://ip-address/upgradefw
// (you first needed to be logged in for the above URL to work)
//
// You can erase all settings by clearing the EEPROM with next URL:
// http://ip-address/erase
// (you first needed to be logged in for the above URL to work)
//
// =========================================================================================

// Current version of the eGarageOpener software
float g_current_version = 1.01;

// -----------------------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------------------

// For ESP8266
#include <ESP8266WiFi.h>                            // ESP8266 WiFi
#include <ESP8266WebServer.h>                       // ESP8266 Web Server
#include <EEPROM.h>                                 // EEPROM
#include <SPI.h>                                    // Serial Peripheral Interrface
#include <FS.h>                                     // File system

// -----------------------------------------------------------------------------------------
// DEFINES
// -----------------------------------------------------------------------------------------

// Defines for EEPROM map
#define LENGTH_SSID                            32   // Maximum length SSID
#define LENGTH_PASSWORD                        64   // Maximum length Password for SSID
#define LENGTH_HOSTNAME                        15   // Maximum length hostname
#define LENGTH_API_KEY                         64   // Maximum length API Key

#define LENGTH_WEB_USER                        64   // Maximum lenght of web-user
#define LENGTH_WEB_PASSWORD                    64   // Maximum lenght of web-password

#define ALLOCATED_EEPROM_BLOCK               1024   // Size of EEPROM block that you claim

//
// Defines for used pins on ESP8266 for controlling motor
//
// Remark: for NodeMCU the hard-coded pin-numbers can be found in file
// \variants\nodemcu\pins_arduino
//
// In this file you will find:
//
// #define PIN_WIRE_SDA (4)
// #define PIN_WIRE_SCL (5)
//
// static const uint8_t SDA = PIN_WIRE_SDA;
// static const uint8_t SCL = PIN_WIRE_SCL;
//
// #define LED_BUILTIN 16
//
//  static const uint8_t D0   = 16;
//  static const uint8_t D1   = 5;
//  static const uint8_t D2   = 4;
//  static const uint8_t D3   = 0;
//  static const uint8_t D4   = 2;
//  static const uint8_t D5   = 14;
//  static const uint8_t D6   = 12;
//  static const uint8_t D7   = 13;
//  static const uint8_t D8   = 15;
//  static const uint8_t D9   = 3;
//  static const uint8_t D10  = 1;
//

#define BUILTIN_LED1             D4       // Builtin LED of ESP8266 is on GPIO2 (=D4)

// Miscellaneous defines
#define g_epsilon           0.00001       // Used for comparing fractional values

// Default language
#define DEFAULT_LANGUAGE          1       // Default language is Dutch (=1), 0=English

// -----------------------------------------------------------------------------------------
// CONSTANTS
// -----------------------------------------------------------------------------------------

// ESP8266 settings
const char*       g_AP_ssid              = "eGarageOpener";     // Default Access Point ssid
const char*       g_AP_password          = "eGarageOpener";     // Default Password for AP ssid

const char*       g_default_host_name    = "eGarageOpener";     // Default Hostname
const char*       g_default_api_key      = "27031969";    // Default API key

const char*       g_default_web_user     = "admin";       // Default Web User
const char*       g_default_web_password = "notdodo";     // Default Web Password

// -----------------------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------------------

// Webserver
ESP8266WebServer  server(80);                       // Listen port Webserver
WiFiClientSecure  client;                           // Client connection to Domoticz

unsigned int      g_start_eeprom_address = 0;       // Start offset in EEPROM

// Structure used for EEPROM read/write
struct {
  
  char  ssid              [ LENGTH_SSID      + 1 ]            = "";
  char  password          [ LENGTH_PASSWORD  + 1 ]            = "";

  float version                                               = 0.0;

  char  hostname          [ LENGTH_HOSTNAME  + 1 ]            = "";
  char  apikey            [ LENGTH_API_KEY   + 1 ]            = "";
  int   apikey_set                                            = 0;

  int   language                                              = DEFAULT_LANGUAGE;

  char  web_user          [ LENGTH_WEB_USER      + 1 ]        = "";
  char  web_password      [ LENGTH_WEB_PASSWORD  + 1 ]        = "";

  int   relay_pin                                             = 14;     // GPIO14 = D5
  int   pulse_time									                          = 300;  	// in milli-seconds

} g_data;

// For upgrading firmware
File    					g_upload_file;
String  					g_filename;

// Running in Access Point (AP) Mode (=0) or Wireless Mode (=1). Default start with AP Mode
int     					g_wifi_mode = 0;

// -----------------------------------------------------------------------------------------
// LANGUAGE STRINGS
// -----------------------------------------------------------------------------------------
const String l_configure_wifi_network[] = {
    "Configure WiFi Network",
    "Configureer WiFi Netwerk",
  };
const String l_enter_wifi_credentials[] = {
    "Provide Network SSID and password",
    "Vul Netwerk SSID en wachtwoord in",
  };
const String l_network_ssid_label[] = {
    "Network SSID       : ",
    "Netwerk SSID       : ",
  };
const String l_network_password_label[] = {
    "Network Password   : ",
    "Netwerk Wachtwoord : ",
  };
  
const String l_network_ssid_hint[] = {
    "network SSID",
    "netwerk SSID",
  };
const String l_network_password_hint[] = {
    "network password",
    "netwerk wachtwoord",  
  };

const String l_login_screen[] = {
    "eGarageOpener Login", 
    "eGarageOpener Login" ,
  };
const String l_provide_credentials[] = { 
    "Please provide a valid user-name and password", 
    "Geef een correcte gebruikersnaam en wachtwoord" ,
  };
const String l_user_label[] = {
    "User       : ",
    "Gebruiker  : ", 
  };
const String l_password_label[] = {
    "Password   : ",
    "Wachtwoord : ",
  };  
  
const String l_username[] = {
    "user-name",
    "gebruikersnaam",
  };
const String l_password[] = {
    "password",
    "wachtwoord",
  };

const String l_settings_screen[] = {
    "eGarageOpener Settings", 
    "eGarageOpener Instellingen",
  };
const String l_provide_new_credentials[] = { 
    "Please provide a new user-name, password and API key", 
    "Geef een nieuwe gebruikersnaam, wachtwoord en API key",
  };

const String l_new_user_label[] = {
    "New User                : ",
    "Nieuwe Gebruiker        : ", 
  };
const String l_new_password_label[] = {
    "New Password            : ",
    "Nieuw Wachtwoord        : ",
  };  
const String l_new_api_key[] = {
    "New API Key             : ",
    "Nieuwe API Key          : ",
  };
  
const String l_change_language_hostname[] = {
    "Change language and Hostname",
    "Verander taal en hostnaam"
  };
const String l_language_label[] = {
    "New Language            : ",
    "Nieuwe Taal             : ",
  };
const String l_hostname_label[] = {
    "New Hostname            : ",
    "Nieuwe Hostnaam         : ",
  };
const String l_language_english[] = {
    "English",
    "Engels",
  };
const String l_language_dutch[] = {
    "Dutch",
    "Nederlands",
  };
const String l_change_settings[] = {
    "Change settings",
    "Verander instellingen",
  };

const String l_login_again [] = {
    "After pressing [Submit] you possibly need to login again",
    "Na het drukken van [Submit] moet je mogelijk opnieuw inloggen",
  };

const String l_version[] = {
    "Version",
    "Versie",
  };

const String l_hostname[] = {
    "Hostname ",
    "Hostnaam ",
  };

const String l_btn_logout[] = {
    "Logout",
    "Uitloggen",
  };
const String l_btn_settings[] = {
    "Settings",
    "Instellingen",
  };

const String l_btn_activate[] = {
    "Activate",
    "Activeer",
  };
  
const String l_btn_stop_updating[] = {
    "Stop updating",
    "Stop updaten",
  };
const String l_btn_start_updating[] = {
    "Start updating",
    "Start updaten",
  };

const String l_after_upgrade [] = {
    "After pressing 'Upgrade' please be patient. Wait until device is rebooted.",
    "Wees geduldig na het drukken van 'Upgrade'. Wacht totdat het device opnieuw is opgestart.",
  };

const String l_change_hardware_settings[] = {
    "Set Hardware related settings for connected devices",
    "Configureer hardware gerelateerde instellingen van aangesloten devices"
  };

const String l_pin_relay_label[] = {
    "GPIO for Relay          : ",
    "GPIO voor Relais        : ",
  };

const String l_pulse_time_label[] = {
    "Pulse-Time (in ms)      : ",
    "Pulse-Tijd (in ms)      : ",
  };

// -----------------------------------------------------------------------------------------
// IMPLEMENTATION OF METHODS    
// -----------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------
// setup
// Default setup() method called once by Arduino
// -----------------------------------------------------------------------------------------
void setup() 
{
  // Setup serial port for your Serial Monitor
  Serial.begin(115200);

  // Initialize SPI
  SPI.begin();

  // Initialize EEPROM before you can use it 
  // (should be enough to fit g_data structure)
  EEPROM.begin( ALLOCATED_EEPROM_BLOCK );

  // Read current contents of EEPROM
  EEPROM.get( g_start_eeprom_address, g_data );

  // Set internal LED as Output
  pinMode(BUILTIN_LED1, OUTPUT);

  // Switch off internal LED
  digitalWrite(BUILTIN_LED1, HIGH);

  delay(200);

  Serial.println("");
  Serial.println("");
  Serial.println("Initializing");
  Serial.println("");

  // If current major version differs from EEPROM, write back new default values
  if ( (int)g_data.version != (int)g_current_version )
  {
    Serial.print("Major version in EEPROM ('");
    Serial.print(g_data.version);
    Serial.print("') differs with this current version ('");
    Serial.print(g_current_version);
    Serial.println("')"), 
    Serial.println("Setting default values in EEPROM.");
    Serial.println("");

    // Write back new default values
    strcpy (g_data.ssid,               "");
    strcpy (g_data.password,           "");
    
    strcpy (g_data.hostname,           g_default_host_name);
    g_data.version                   = g_current_version;
    strcpy (g_data.apikey,             g_default_api_key);
    g_data.apikey_set                = 0;

    strcpy(g_data.web_user,            g_default_web_user);
    strcpy(g_data.web_password,        g_default_web_password);
    
    g_data.language                  = DEFAULT_LANGUAGE;   

    g_data.relay_pin                 = 14;      // D5: GPIO14
    g_data.pulse_time                = 300;     // (in ms)
    
  }

  // Set current version
  g_data.version = g_current_version;

  // Store values into EEPROM
  EEPROM.put( g_start_eeprom_address, g_data );
  EEPROM.commit();

  // Display current set hostname
  Serial.print("EEPROM Hostname                 : ");
  Serial.println(g_data.hostname);

  // Set Hostname
  WiFi.hostname( g_data.hostname );    

  // Display current 'eGarageOpener' version
  Serial.print("EEPROM Version                  : ");
  Serial.println(g_data.version);

  // Display current Web-Page Language Setting
  Serial.print("EEPROM Language                 : ");

  if (g_data.language == 0)
  {
    Serial.println(l_language_english[g_data.language]);
  }
  else
  {
    Serial.println(l_language_dutch[g_data.language]);
  }
  
  // Display current API key
  Serial.print("EEPROM API key                  : ");
  Serial.println(g_data.apikey);
  Serial.print("EEPROM API key set              : ");
  Serial.println(g_data.apikey_set);

  Serial.println("");

  // Display set web-user & web-password
  Serial.print("EEPROM Web User                 : ");
  Serial.println(g_data.web_user);
  Serial.print("EEPROM Web Password             : ");
  Serial.println(g_data.web_password);
  Serial.println("");

  // Display stored SSID & password
  Serial.print("EEPROM ssid                     : ");
  Serial.println(g_data.ssid);
  Serial.print("EEPROM password                 : ");
  Serial.println(g_data.password);
  Serial.println();

  Serial.print("EEPROM Relay Pin                : ");
  Serial.println(g_data.relay_pin);
  Serial.print("EEPROM Pulse-Time               : ");
  Serial.println(g_data.pulse_time);
  Serial.println();

  // AP or WiFi Connect Mode

  // No WiFi configuration, then default start with AP mode
  if ( (strlen(g_data.ssid) == 0) || (strlen(g_data.password) == 0) )
  {
    Serial.println("No WiFi configuration found, starting in AP mode");
    Serial.println("");

    Serial.print("SSID          : '");
    Serial.print(g_AP_ssid);
    Serial.println("'");
    Serial.print("Password      : '");
    Serial.print(g_AP_password);
    Serial.println("'");

    // Ask for WiFi network to connect to and password
    g_wifi_mode = 0;
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(g_AP_ssid, g_AP_password);

    Serial.println("");
    Serial.print("Connected to  : '");
    Serial.print(g_AP_ssid);
    Serial.println("'");
    Serial.print("IP address    : ");
    Serial.println(WiFi.softAPIP());
  }
  // WiFi configuration found, try to connect
  else
  {
    int i = 0;

    Serial.print("Connecting to : '");
    Serial.print(g_data.ssid);
    Serial.println("'");

    WiFi.mode(WIFI_STA);
    WiFi.begin(g_data.ssid, g_data.password);

    while (WiFi.status() != WL_CONNECTED && i < 31)
    {
      delay(1000);
      Serial.print(".");
      ++i;
    }

    // Unable to connect to WiFi network with current settings
    if (WiFi.status() != WL_CONNECTED && i >= 30)
    {
      WiFi.disconnect();

      g_wifi_mode = 0;

      delay(1000);
      Serial.println("");

      Serial.println("Couldn't connect to network :( ");
      Serial.println("Setting up access point");
      Serial.print("SSID     : '");
      Serial.print(g_AP_ssid);
      Serial.println("'");
      Serial.print("Password : ");
      Serial.println(g_AP_password);
      
      // Ask for WiFi network to connect to and password
      WiFi.mode(WIFI_AP);
      WiFi.softAP(g_AP_ssid, g_AP_password);
      
      Serial.print("Connected to  : '");
      Serial.println(g_AP_ssid);
      Serial.println("'");
      IPAddress myIP = WiFi.softAPIP();
      Serial.print("IP address    : ");
      Serial.println(myIP);
    }
    // Normal connecting to WiFi network
    else
    {
      g_wifi_mode = 1;

      Serial.println("");
      Serial.print("Connected to  : '");
      Serial.print(g_data.ssid);
      Serial.println("'");
      Serial.print("IP address    : ");
      Serial.println(WiFi.localIP());
      
      // Disable sleep Mode
      // WiFi.setSleepMode(WIFI_NONE_SLEEP);      
    }
  }

  // If connected to AP mode, display simple WiFi settings page
  if ( g_wifi_mode == 0)
  {
      Serial.println("");
      Serial.println("Connected in AP mode thus display simple WiFi settings page");
      server.on("/", handle_wifi_html);
  }
  // If connected in WiFi mode, display the eGarageOpener Web-Interface
  else
  {
    Serial.println("");
    Serial.println("Connected in WiFi mode thus display eGarageOpener Web-Interface");
    server.on ( "/", handle_root_ajax ); 
  }

  // Setup supported URL's for this ESP8266
  server.on("/erase", handle_erase );                       // Erase EEPROM
  server.on("/api", handle_api);                            // API of eGarageOpener

  server.on("/wifi_ajax", handle_wifi_ajax);                // Handle simple WiFi dialog
  
  server.on("/login", handle_login_html);                   // Login Screen
  server.on("/login_ajax", handle_login_ajax);              // Handle Login Screen

  server.on("/settings", handle_settings_html);             // Change Settings
  server.on("/settings_ajax", handle_settings_ajax);        // Handle Change Settings

  server.on("/upgradefw", handle_upgradefw_html);           // Upgrade firmware
  server.on("/upgradefw2", HTTP_POST, []() 
  {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() 
  {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START)
    {
      g_filename = upload.filename;
      Serial.setDebugOutput(true);
      
      Serial.printf("Starting upgrade with filename: %s\n", upload.filename.c_str());
      Serial.printf("Please be patient...\n");
      
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x01000) & 0xFFFFF000;

      // Start with max available size
      if ( !Update.begin( maxSketchSpace) ) 
      { 
          Update.printError(Serial);
      }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
          Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true)) //true to set the size to the current progress
        {
          Serial.printf("Upgrade Success: %u\nRebooting...\n", upload.totalSize);
        }
        else
        {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
    }
    yield();
  });

  // List of headers to be recorded  
  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);

  // Ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize );

  // Set configured relay-pin to INPUT to make sure it is not accidentally triggered during power-failure
  pinMode(g_data.relay_pin, INPUT);

  // Start HTTP server
  server.begin();
  Serial.println("HTTP server started");  
}

// -----------------------------------------------------------------------------------------
// loop ()
// Here you find the main code which is run repeatedly
// -----------------------------------------------------------------------------------------
void loop() 
{
  // Handle Client
  server.handleClient();

  // Flash internal LED
  flash_internal_led();

  // Check if WiFi is still connected
  if ( 
       ( g_wifi_mode == 1)
       &&
       (WiFi.status() != WL_CONNECTED )
     )
  {
//    wifi_reconnect();
  }
}

// -----------------------------------------------------------------------------------------
// wifi_reconnect
// Automatically reconnect to your known WiFi network in case you got disconnected
// -----------------------------------------------------------------------------------------
void wifi_reconnect()
{
  // Try to disconnect WiFi
  Serial.println("WiFi disconnected");
  WiFi.disconnect(true);

  // Starting WiFi again
  Serial.println("Starting WiFi again");

  Serial.print("Connecting to : '");
  Serial.print(g_data.ssid);
  Serial.println("'");

  WiFi.mode(WIFI_STA);
  WiFi.begin(g_data.ssid, g_data.password);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i < 31)
  {
    delay(500);
    Serial.print(".");
    ++i;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
      Serial.println("");
      Serial.print("Connected to  : '");
      Serial.print(g_data.ssid);
      Serial.println("'");
      Serial.print("IP address    : ");
      Serial.println(WiFi.localIP());
  }
}

// -----------------------------------------------------------------------------------------
// handle_api
// API commands you can send to the Webserver in format: action, value, api. 
// (because of security reasons the 'api' is mandatory)
//
// Example: http://ip-address/api?action=set_host&value=my_host
//
// Currently next API actions are supported:
//
// ESP8266 Functions:
// reboot, value              // Reboot in case 'value' = "true"
// reset, value               // Erase EEPROM in case 'value' == "true"
//
// Miscellaneous Functions:
// set_api, value             // Set new API key to 'value'
// set_host, value            // Set hostname to 'value'
// set_language, value        // Set Web-Interface Language (0=English, 1=Dutch)
//
// -----------------------------------------------------------------------------------------
void handle_api()
{
  // Get variables for all commands (action,value,api)
  String action = server.arg("action");
  String value = server.arg("value");
  String api = server.arg("api");

  char buffer[256];
  sprintf( buffer, "\nWebserver API called with parameters (action='%s', value='%s', api='%s')\n\n", action.c_str(), value.c_str(), api.c_str() );

  Serial.print( buffer );

  // First check if user wants to set new API key (only possible when not already set)
  if ( (action == "set_api") && (g_data.apikey_set == 0) && (strcmp(api.c_str(), g_default_api_key)) )
  {
    Serial.println("Handle 'set_api' action");

    // Flash internal LED
    flash_internal_led();

    // Make sure that API key is not bigger as 64 characters (= LENGTH_API_KEY)
    if ( strlen(api.c_str()) > LENGTH_API_KEY )
    {
      char buffer[256];
      sprintf( buffer, "NOK (API key '%s' is bigger then '%d' characters)", api.c_str(), LENGTH_API_KEY );
      
      server.send ( 501, "text/html", buffer);
      delay(200);
    }
    else
    {
      Serial.print("Setting API key to value: '");
      Serial.print(value);
      Serial.println("'");
  
      strcpy( g_data.apikey, value.c_str() );
      g_data.apikey_set = 1;
  
      // replace values in EEPROM
      EEPROM.put( g_start_eeprom_address, g_data);
      EEPROM.commit();
  
      char buffer[256];
      sprintf( buffer, "OK (API key set to value '%s')", g_data.apikey );
      
      server.send ( 200, "text/html", buffer);
      delay(200);
    }
  }

  // API key valid?
  if (strcmp (api.c_str(), g_data.apikey) != 0 )
  {
    char buffer[256];
    sprintf( buffer, "NOK (you are not authorized to perform an action without a valid API key, provided api-key '%s')", api.c_str() );
    
    server.send ( 501, "text/html", buffer);
    delay(200);
  }
  else
  {
    // Action: reboot
    if (action == "reboot")
    {
      Serial.println("Handle 'reboot' action");

      server.send ( 200, "text/html", "OK");
      delay(500);

      // Flash internal LED
      flash_internal_led();

      if ( strcmp(value.c_str(), "true") == 0 )
      {
        Serial.println("Rebooting ESP8266");
        ESP.restart();
      }
    }
    // Action: reset
    else if (action == "reset")
    {
      Serial.println("Handle 'reset' action");

      server.send ( 200, "text/html", "OK");
      delay(200);

      if ( strcmp(value.c_str(), "true") == 0 )
      {
        // Flash internal LED
        flash_internal_led();
        
        Serial.println("Clearing EEPROM values of ESP8266");

        // Fill used EEPROM block with 00 bytes
        for (unsigned int i = g_start_eeprom_address; i < (g_start_eeprom_address + ALLOCATED_EEPROM_BLOCK); i++)
        {
          EEPROM.put(i, 0);
        }    
        // Commit changes to EEPROM
        EEPROM.commit();
  
        ESP.restart();
      }
    }
    // Action: set_host
    else if (action == "set_host")
    {
      Serial.println("Handle 'set_host' action");

      // Make sure that value is supplied for hostname
      if ( value == "" )
      {
        Serial.println("No hostname supplied for 'set_host' action");
        
        server.send ( 501, "text/html", "NOK (no hostname supplied for 'set_host' action)");
        delay(200);
      }
      else
      {
        // Flash internal LED
        flash_internal_led();
        
        // Set new Hostname
        strcpy( g_data.hostname, value.c_str() );
  
        // Replace values in EEPROM
        EEPROM.put( g_start_eeprom_address, g_data);
        EEPROM.commit();
  
        Serial.print("Setting hostname to: '");
        Serial.print(value);
        Serial.println("'");
  
        // Set Hostname
        WiFi.hostname(value);         

        char buffer[256];
        sprintf( buffer, "OK (hostname set to value '%s')", value.c_str() );
        
        server.send ( 200, "text/html", buffer);
        delay(200);
      }
    }
    // Action: set_language
    else if (action == "set_language")
    {
      Serial.println("Handle 'set_language' action");

      // Make sure that value is supplied for hostname
      if (value == "")
      {
        Serial.println("No language supplied for 'set_language' action, falling back to English");

        g_data.language = 0;
        
        // Replace values in EEPROM
        EEPROM.put( g_start_eeprom_address, g_data);
        EEPROM.commit();
       
        server.send ( 501, "text/html", "NOK (no language supplied for 'set_language' action, falling back to English");
        delay(200);
      }
      else if ( (value.toInt() < 0) || (value.toInt() > 1) )
      {
        Serial.println("Unknown language supplied for 'set_language' action, falling back to English");

        g_data.language = 0;
        
        // Replace values in EEPROM
        EEPROM.put( g_start_eeprom_address, g_data);
        EEPROM.commit();
       
        server.send ( 501, "text/html", "NOK (unknown language supplied for 'set_language' action, falling back to English");
        delay(200);
      }
      else
      {
        // Flash internal LED
        flash_internal_led();

        g_data.language = value.toInt();
        
        // Replace values in EEPROM
        EEPROM.put( g_start_eeprom_address, g_data);
        EEPROM.commit();

        char buffer [256];
        sprintf( buffer, "OK (language set to value '%d')", g_data.language );

        server.send ( 200, "text/html", buffer);
        delay(200);
      }
    }   
    else if (action == "relay")
    {
      Serial.println("Handle 'relay' action");

      // Activate Relay
      pinMode(g_data.relay_pin, OUTPUT);
      digitalWrite(g_data.relay_pin, HIGH);
    
      // Wait configured milli-seconds
      delay(value.toInt());
    
      // De-Activate Relay
      digitalWrite(g_data.relay_pin, LOW);
      pinMode(g_data.relay_pin, INPUT);
    }
    
    // Unknown action supplied :-(
    else
    {
      Serial.println("Unknown action");
      server.send ( 501, "text/html", "NOK (unknown action supplied)");      
    }
  }
}

// -----------------------------------------------------------------------------------------
// flash_internal_led()
// Flash internal LED of ESP8266 board
// -----------------------------------------------------------------------------------------
void flash_internal_led()
{
  // Set internal LED on
  digitalWrite(BUILTIN_LED1, LOW);
  delay(10);

  // Set internal LED off 
  digitalWrite(BUILTIN_LED1, HIGH);
  delay(10);
}

// -----------------------------------------------------------------------------------------
// handle_erase
// Erase the EEPROM contents of the ESP8266 board
// -----------------------------------------------------------------------------------------
void handle_erase()
{
  server.send ( 200, "text/html", "OK");
  Serial.println("Erase complete EEPROM");

  for (unsigned int i = 0 ; i < ALLOCATED_EEPROM_BLOCK; i++) 
  {
    EEPROM.write(i, 0);
  }  
  // Commit changes to EEPROM
  EEPROM.commit();

  // Restart board
  ESP.restart();
}

// -----------------------------------------------------------------------------------------
// is_authenticated(int set_cookie)
// Helper-method for authenticating for Web Access
// -----------------------------------------------------------------------------------------
bool is_authenticated(int set_cookie)
{
  MD5Builder md5;

  // Create unique MD5 value (hash) of Web Password
  md5.begin();
  md5.add(g_data.web_password);
  md5.calculate();

  String web_pass_md5 = md5.toString();
  String valid_cookie = "eGarageOpener=" + web_pass_md5;

  // Check if web-page has Cookie data
  if (server.hasHeader("Cookie"))
  {
    // Check if Cookie of web-session corresponds with our Cookie of eGarageOpener
    String cookie = server.header("Cookie");

    if (cookie.indexOf(valid_cookie) != -1)
    {
      // No, then set our Cookie for this Web session
      if (set_cookie == 1)
      {
        server.sendHeader("Set-Cookie","eGarageOpener=" + web_pass_md5 + "; max-age=86400");
      }
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------------------
// handle_login_html()
// -----------------------------------------------------------------------------------------
void handle_login_html()
{
  String login_html_page;

  login_html_page  = "<title>" + l_login_screen[g_data.language] + "</title>";
  login_html_page += "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";

  login_html_page += "</head><body><h1>" + l_login_screen[g_data.language] + "</h1>";

  login_html_page += "<html><body>";
  login_html_page += "<form action='/login_ajax' method='POST'>" + l_provide_credentials[g_data.language];
  login_html_page += "<br>";
  login_html_page += "<input type='hidden' name='form' value='login'>";
  login_html_page += "<pre>";

  char length_web_user[10];
  sprintf( length_web_user, "%d", LENGTH_WEB_USER );
  String str_length_web_user = length_web_user;

  char length_web_password[10];
  sprintf( length_web_password, "%d", LENGTH_WEB_PASSWORD );
  String str_length_web_password = length_web_password;
 
  login_html_page += l_user_label[g_data.language] + "<input type='text' name='user' placeholder='" + l_username[g_data.language] + "' maxlength='" + str_length_web_user + "'>";
  login_html_page += "<br>";
  login_html_page += l_password_label[g_data.language] + "<input type='password' name='password' placeholder='" + l_password[g_data.language] + "' maxlength='" + str_length_web_password + "'>";
  login_html_page += "</pre>";
  login_html_page += "<input type='submit' name='Submit' value='Submit'></form>";
  login_html_page += "<br><br>";
  login_html_page += "</body></html>";
  
  server.send(200, "text/html", login_html_page);
}

// -----------------------------------------------------------------------------------------
// handle_login_ajax()
// -----------------------------------------------------------------------------------------
void handle_login_ajax()
{
  String form   = server.arg("form");
  String action = server.arg("action");
  
  if (form == "login")
  {
    String user_arg = server.arg("user");
    String pass_arg = server.arg("password");

    // Calculate MD5 (hash) of set web-password in EEPROM
    MD5Builder md5;   
    md5.begin();
    md5.add(g_data.web_password);
    md5.calculate();
    String web_pass_md5 = md5.toString();

    // Check if Credentials are OK and if yes set the Cookie for this session
    if (user_arg == g_data.web_user && pass_arg == g_data.web_password)
    {
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eGarageOpener=" + web_pass_md5 + "; max-age=86400\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
    }
    else
    {
      Serial.println("Wrong user-name and/or password supplied");

      // Reset Cookie and present login screen again
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eGarageOpener=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
    }
  }
  else
  {
    server.send ( 200, "text/html", "Nothing");
  }
}

// -----------------------------------------------------------------------------------------
// handle_settings_html()
// -----------------------------------------------------------------------------------------
void handle_settings_html()
{ 
  // Check if you are authorized to see the Settings dialog
  if ( !is_authenticated(0) )
  {
    Serial.println("Unauthorized to change Settings. Reset cookie and present login screen");

    // Reset Cookie and present login screen
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eGarageOpener=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    
    server.send ( 200, "text/html", "Unauthorized");
  }
  else
  {   
    String settings_html_page;
  
    settings_html_page  = "<title>" + l_settings_screen[g_data.language] + "</title>";
    settings_html_page += "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
  
    settings_html_page += "</head><body><h1>" + l_settings_screen[g_data.language] + "</h1>";
  
    settings_html_page += "<html><body>";
    settings_html_page += "<form action='/settings_ajax' method='POST'>";
    settings_html_page += l_provide_new_credentials[g_data.language];
    settings_html_page += "<br>";
    settings_html_page += "<input type='hidden' name='form' value='settings'>";
    settings_html_page += "<pre>";

    char length_web_user[10];
    sprintf( length_web_user, "%d", LENGTH_WEB_USER );
    String str_length_web_user = length_web_user;

    char length_web_password[10];
    sprintf( length_web_password, "%d", LENGTH_WEB_PASSWORD );
    String str_length_web_password = length_web_password;

    char length_api_key[10];
    sprintf( length_api_key, "%d", LENGTH_API_KEY );
    String str_length_api_key = length_api_key;
    
    settings_html_page += l_new_user_label[g_data.language] + "<input type='text' name='user' value='";
    settings_html_page += g_data.web_user;
    settings_html_page += "' maxlength='" + str_length_web_user + "'>";
    settings_html_page += "<br>";
    settings_html_page += l_new_password_label[g_data.language] + "<input type='text' name='password' value='";
    settings_html_page += g_data.web_password;
    settings_html_page += "' maxlength='" + str_length_web_password + "'>";
    settings_html_page += "<br>";
    settings_html_page += l_new_api_key[g_data.language] + "<input type='text' name='apikey' value='";
    settings_html_page += g_data.apikey;
    settings_html_page += "' maxlength='" + str_length_api_key + "'>";
    settings_html_page += "</pre>";
    settings_html_page += "<br>";

    settings_html_page += l_change_language_hostname[g_data.language];
    settings_html_page += "<br>";   
    settings_html_page += "<pre>";
    settings_html_page += l_language_label[g_data.language] + "<select name='language'>";

    // Default English
    if (g_data.language == 0)
    {
      settings_html_page += "<option selected>";
      settings_html_page += l_language_english[g_data.language];
      settings_html_page += "</option><option>";
      settings_html_page += l_language_dutch[g_data.language];
      settings_html_page += "</option></select>";
    }
    // Otherwise Dutch
    else
    {
      settings_html_page += "<option>";
      settings_html_page += l_language_english[g_data.language];
      settings_html_page += "</option><option selected>";
      settings_html_page += l_language_dutch[g_data.language];
      settings_html_page += "</option></select>";
    }

    char length_hostname[10];
    sprintf( length_hostname, "%d", LENGTH_HOSTNAME );
    String str_length_hostname = length_hostname;

    settings_html_page += "<br>";
    settings_html_page += l_hostname_label[g_data.language] + "<input type='text' name='hostname' value='";
    settings_html_page += g_data.hostname;
    settings_html_page += "' maxlength='" + str_length_hostname + "'>"; 
    settings_html_page += "</pre>";

    settings_html_page += "<br>";

    settings_html_page += l_change_hardware_settings[g_data.language];
    settings_html_page += "<br>";   
    settings_html_page += "<pre>";   

    settings_html_page += l_pin_relay_label[g_data.language] + "<input type='text' name='relay_pin' value='";
    settings_html_page += g_data.relay_pin;
    settings_html_page += "'>"; 
    settings_html_page += "<br>";

    settings_html_page += l_pulse_time_label[g_data.language] + "<input type='text' name='pulse_time' value='";
    settings_html_page += g_data.pulse_time;
    settings_html_page += "'>"; 
    settings_html_page += "<br>";
    settings_html_page += "</pre>";   
    settings_html_page += "<br>";

    settings_html_page += l_login_again[g_data.language];
    settings_html_page += "<br><br>";
     
    settings_html_page += "<input type='submit' name='Submit' value='Submit'></form>";
    settings_html_page += "<br><br>";
    settings_html_page += "</body></html>";
    
    server.send(200, "text/html", settings_html_page);
  }
}
  
// -----------------------------------------------------------------------------------------
// handle_settings_ajax()
// -----------------------------------------------------------------------------------------
void handle_settings_ajax()
{
  String form   = server.arg("form");
  String action = server.arg("action");
  
  if (form == "settings")
  {
    String user_arg           = server.arg("user");
    String pass_arg           = server.arg("password");
    String api_arg            = server.arg("apikey");
    
    String host_arg           = server.arg("hostname");
    String lang_arg           = server.arg("language");

    String relay_pin_arg      = server.arg("relay_pin");
    String pulse_time_arg     = server.arg("pulse_time");
 
    // Check if new web-user of web-password has been set
    int login_again = 0;
    if ( (strcmp (g_data.web_user, user_arg.c_str()) != 0) || (strcmp(g_data.web_password, pass_arg.c_str()) != 0) )
    {
      login_again = 1;
    }
    
    // Set new Web User and Web Password
    strcpy( g_data.web_user,     user_arg.c_str() );
    strcpy( g_data.web_password, pass_arg.c_str() );
    strcpy( g_data.hostname,     host_arg.c_str() );

    // Set new API key
    strcpy( g_data.apikey,       api_arg.c_str() );
    
    // Set new language
    if ( strcmp( lang_arg.c_str(), l_language_english[g_data.language].c_str()) == 0 )
    {
      g_data.language = 0;
    }
    else
    {
      g_data.language = 1;
    }

    // Set Hostname
    WiFi.hostname(g_data.hostname);         

    // Hardware settings
    g_data.relay_pin  = relay_pin_arg.toInt();
    g_data.pulse_time = pulse_time_arg.toInt();

    // Set configured relay-pin to INPUT to make sure it is not accidentally triggered during power-failure
    pinMode(g_data.relay_pin, INPUT);
   
    // Update values in EEPROM
    EEPROM.put( g_start_eeprom_address, g_data);
    EEPROM.commit();

    char buffer[256];
    sprintf(buffer,"New webuser-name='%s', new webuser-password='%s'", g_data.web_user, g_data.web_password);
    Serial.println(buffer);

    // Force login again in case web-user or web-password is changed
    if (login_again == 1)
    {
      // Reset Cookie and present login screen again
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eGarageOpener=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
    }
    // Otherwise present main Web-Interface again
    else
    {
      String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);      
    }
  }
}

// -----------------------------------------------------------------------------------------
// get_page
// Construct eGarageOpener Web-Interface
// -----------------------------------------------------------------------------------------
String get_page()
{ 
  String  page;
  page  = "<html lang=nl-NL><head>";  

  page += "<title>eGarageOpener</title>";
  page += "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
  page += "</head>";
  
  page += "<body><h1>eGarageOpener Web-Interface</h1>";

  page += l_version[g_data.language] + " '";
  page += g_data.version;
  page += "', ";
  
  page += l_hostname[g_data.language] + "'";
  page += g_data.hostname;
  page += "'";
  page += "<br>";
  page += "<br>";

  page += "<form action='/' method='POST'>";
  page += "<input type='submit' name='logout' value='" + l_btn_logout[g_data.language] + "'>";
  page += "<input type='submit' name='settings' value='" + l_btn_settings[g_data.language] + "'>";

  page += "<br>";
  page += "<br>";

  page += "<input type='submit' name='activate' style='height:100px; width:200px; font-size:40px' value='" + l_btn_activate[g_data.language] + "'</div>";
  
  page += "</body></html>";

  return (page);
}

// -----------------------------------------------------------------------------------------
// handle_root_ajax
// Handle user-interface actions executed on the Web-Interface
// -----------------------------------------------------------------------------------------
void handle_root_ajax()
{
  // Check if you are authorized to see the web-interface
  if ( !is_authenticated(0) )
  {
    Serial.println("Unauthorized to access Web-Interface. Reset Cookie andp present login screen.");

    // Reset Cookie and present login screen
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eGarageOpener=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    
    server.send ( 200, "text/html", "Unauthorized");
  }
  else
  {
    // Logout
    if (server.hasArg("logout") )
    {
      handle_logout();
    }
    // Change Credentials
    else if (server.hasArg("settings") )
    {
      handle_settings();
    }
    else if (server.hasArg("activate") )
    {
      handle_activate();      
    }
    else
    {
      server.send ( 200, "text/html", get_page() ); 
    }
  }
}

// -----------------------------------------------------------------------------------------
// handle_upgradefw_html
// Handle upgrading of firmware of ESP8266 device
// -----------------------------------------------------------------------------------------
void handle_upgradefw_html()
{
  // Check if you are authorized to upgrade the firmware
  if ( !is_authenticated(0) )
  {
    Serial.println("Unauthorized to upgrade firmware");

    // Reset Cookie and present login screen
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eGarageOpener=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    
    server.send ( 200, "text/html", "Unauthorized");
  }
  else
  {
    String upgradefw_html_page;
  
    upgradefw_html_page  = "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
    upgradefw_html_page += "</head><body><h1>Upgrade firmware</h1>";
    upgradefw_html_page += "<br>";
    upgradefw_html_page += "<form method='POST' action='/upgradefw2' enctype='multipart/form-data'>";
    upgradefw_html_page += "<input type='file' name='upgrade'>";
    upgradefw_html_page += "<input type='submit' value='Upgrade'>";
    upgradefw_html_page += "</form>";
    upgradefw_html_page += "<b>" + l_after_upgrade[g_data.language] + "</b>";
  
    server.send ( 200, "text/html", upgradefw_html_page);
  }
}

// -----------------------------------------------------------------------------------------
// handle_wifi_html
// Simple WiFi setup dialog
// -----------------------------------------------------------------------------------------
void handle_wifi_html()
{
  String wifi_page;

  wifi_page  = "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
  wifi_page += "</head><body><h1>" + l_configure_wifi_network[g_data.language] + "</h1>";
  wifi_page += "<html><body>";
  wifi_page += "<form action='/wifi_ajax' method='POST'>" + l_enter_wifi_credentials[g_data.language];
  wifi_page += "<br>";  
  wifi_page += "<input type='hidden' name='form' value='wifi'>";  

  char length_ssid[10];
  sprintf( length_ssid, "%d", LENGTH_SSID );
  String str_length_ssid = length_ssid;

  char length_ssid_password[10];
  sprintf( length_ssid_password, "%d", LENGTH_PASSWORD );
  String str_length_ssid_password = length_ssid_password;
  
  wifi_page += "<pre>";
  wifi_page += l_network_ssid_label[g_data.language] + "<input type='text' name='ssid' placeholder='" + l_network_ssid_hint[g_data.language] + "' maxlength='" + str_length_ssid + "'>";
  wifi_page += "<br>";
  wifi_page += l_network_password_label[g_data.language] + "<input type='password' name='password' placeholder='" + l_network_password_hint[g_data.language] + "' maxlength='" + str_length_ssid_password + "'>";
  wifi_page += "</pre>";
  wifi_page += "<input type='submit' name='Submit' value='Submit'></form>";
  wifi_page += "<br><br>";
  wifi_page += "</body></html>";

  server.send ( 200, "text/html", wifi_page);
}

// -----------------------------------------------------------------------------------------
// handle_wifi_ajax
// Handle simple WiFi setup dialog configuration
// -----------------------------------------------------------------------------------------
void handle_wifi_ajax()
{
  String form = server.arg("form");

  if (form == "wifi")
  {
    String ssidArg = server.arg("ssid");
    String passArg = server.arg("password");

    strcpy( g_data.ssid, ssidArg.c_str() );
    strcpy( g_data.password, passArg.c_str() );

    // Update values in EEPROM
    EEPROM.put( g_start_eeprom_address, g_data);
    EEPROM.commit();
  
    server.send ( 200, "text/html", "OK");
    delay(500);
  
    ESP.restart();
  }
}

// -----------------------------------------------------------------------------------------
// handle_logout
// Logout from Web-Server
// -----------------------------------------------------------------------------------------
void handle_logout()
{
  Serial.println("[Logout] pressed");

  // Reset Cookie and present login screen
  String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eGarageOpener=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";

  server.sendContent(header);
}

// -----------------------------------------------------------------------------------------
// handle_settings
// Change settings for Web-Server
// -----------------------------------------------------------------------------------------
void handle_settings()
{
  Serial.println("[Settings] pressed");
 
  // Present the 'Settings' screen
  String header = "HTTP/1.1 301 OK\r\nLocation: /settings\r\nCache-Control: no-cache\r\n\r\n";

  server.sendContent(header);
}

// -----------------------------------------------------------------------------------------
// handle_activate
// Pressing [Activate] button
// -----------------------------------------------------------------------------------------
void handle_activate()
{
  Serial.println("[Activate] pressed");

  // Flash internal LED
  flash_internal_led();

  // Activate Relay
  pinMode(g_data.relay_pin, OUTPUT);
  digitalWrite(g_data.relay_pin, HIGH);

  // Wait configured milli-seconds
  delay(g_data.pulse_time);

  // De-Activate Relay
  digitalWrite(g_data.relay_pin, LOW);
  pinMode(g_data.relay_pin, INPUT);

  server.send ( 200, "text/html", get_page() );
}

// -----------------------------------------------------------------------------------------
// History of 'eGarageOpener' software
//
// 08-Mar-2019  V1.00   Initial version
// 30-Jun 2019  V1.01   Change default GPIO from 4 to 14.
// -----------------------------------------------------------------------------------------
