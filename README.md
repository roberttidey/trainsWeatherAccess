# UK train and weather Display
Enhanced version of trainsAccess Display

Displays current departures from a particular UK rail station
Displays 5 day weather forecast for worldide cities

This replaces trainsAccess
If you want trains only usage set the line in trainsWeatherConfig.txt for the weather URL to null
If you want weather only usage set the line in trainsWeatherConfig.txt for the trains URL to null AND in the ino file change the dataMode = 0 to dataMode = 1 

## Features
- Accesses National Rail station database to get departure times for a specified station
- Can filter list to show trains going to specific destination
- Accesses openWeather database to get 5 day forecast
- ESP8266 based processing, hooks onto local wifi network
- Battery powered (rechargeable LIPO) with inbuilt charger
- Very low quiescent current (< 20uA) for long battery life
- 320 x 240 LCD display with 3 control buttons
- Normally the module sleeps after a time but can be turned into a non sleep mode for checking and configuration
- Configuration data in a file to allow set up of major parameters
- Web based file access
- Browser access to view data and edit config
- Software can be updated via web interface
- Low cost
- 3D printed enclosure

## Library details
- Uses standard libraries plus TFT_eSPI for fast display control
- TFT_eSPI can be loaded from Arduino library manager
- Modify following in User_Setup.h in TFT_esp library to reflect actual pin usage

- #define TFT_CS   PIN_D8  // Chip select control pin D8
- #define TFT_DC   PIN_D2  // Data Command control pin
- #define TFT_RST  PIN_D4  // Reset pin (could connect to NodeMCU RST, see next line)


## Set up
- Use BaseSuoort library https://github.com/roberttidey/BaseSupport
- Modify BaseConfig.h password fields to suit personal needs
- Uncomment FASTCONNECT in BaseConfig.h as required 
- Compile with Arduino IDE. Memory should be set to 2MB firmware / 2MB Spiffs to allow OTA updates
- Serial upload
- Use the WifiManager AP to select and set up wifi connection
- Register with National Rail and get access token for OpenLDBWS (see link below)
- Register with openweather.org and get access token
- Modify trainsWeatherConfig.txt
	- Change Access tokens
	- Change codes for local station (see link below)
	- Change codes for destination or leave blank for all destinations
	- Change city codes for weather
	- Change refresh and sleep settings as required
- Upload the files in data folder using http:ip/upload
- Further file access can use http:ip/edit
- For software updating export a binary and then access http:/ip/firmware
- Note tha thte site fingerprint for https access may change if the security certificate of the National rail site or weather site is renewed. See below, You can also leave this line blank to skip the certificate check.

## Usage
- Middle button is used to turn unit on. It will automatically get current times and refresh at the specified interval
- Short press of Middle button toggles between train and weather
- It will sleep automatically after the configured time if enabled
- Sleep may be manually forced by a long press of the middle button
- If there are more listings than the display size then a short press of the top and bottom buttons will page up and down through the screens.
- Multiple station, destination codes, and weather cities (up to 4) may be put in the config file (comma separated). A long press of the top bottom will cycle through these combinations.

## URLS
- ip/firmware (update firmware)
- ip/getData (returns current data in CSV format) 
- ip/setMode?mode=n (sets mode n=1 trains, n=2 weather)
- ip Gives Browser access to train or weather data and config

## Certificate fingerprints
- To get the fingerprints, access the raw site url from a browser
	- https://lite.realtime.nationalrail.co.uk/OpenLDBWS/ldb11.asmx
	- https://api.openweathermap.org/data/2.5/forecast
- View site information (icon to the left of the url)
- Click connection is secure/certificate is valid/Details
- Click export, choose a location to stotr certificate file.
- generate fingerprint using openssl
	- For windows open cmd window, cd yo folder where certificate files are
	- "C:\Program Files\Git\usr\bin\openssl" x509 -fingerprint -noout -in [certificateMame].crt
- Copy and paste fingerprints into the config file

## Links
Instructables https://www.instructables.com/id/UK-Train-Display/

Thingiverse Enclosure https://www.thingiverse.com/thing:3843509

National Rail registration https://www.nationalrail.co.uk/100296.aspx

Station codes https://www.nationalrail.co.uk/stations_destinations/48541.aspx


