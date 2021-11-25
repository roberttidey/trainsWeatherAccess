/*
 R. J. Tidey 2019/08/22
 Train times and weather display
 Designed to run on battery with deep sleep after a timeoutbut can be overridden for maintenance
 WifiManager can be used to config wifi network
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */
#define ESP8266
#include "BaseConfig.h"
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include "weatherIcons.h"

#define POWER_HOLD_PIN 16
#define KEY1 0
#define KEY2 1
#define KEY3 2
#define LONG_PRESS 1000
int pinInputs[3] = {0,12,5};
int pinStates[3];
unsigned long pinTimes[3];
int pinChanges[3];
unsigned long pinTimers[3];

//weather decoding response
#define ABS_ZERO 273.16
#define WEATHER_DT "dt"
#define WEATHER_TIME "dt_txt"
#define WEATHER_TEMP "temp"
#define WEATHER_MAIN "main"
#define WEATHER_DESCRIPTION "description"
#define WEATHER_RAIN "rain"
#define WEATHER_WIND "speed"
#define WEATHER_CLOUD "all"
#define WEATHER_ICON "icon"

//train decoding response
#define SERVICE_START "service"
#define SERVICE_TIME "std"
#define SERVICE_EXPECTED "etd"
#define SERVICE_PLATFORM "platform"
#define SERVICE_ORIGIN "origin"
#define SERVICE_DESTINATION "destination"
#define SERVICE_LOCATION "locationName"

#define RESPONSE_FINDTRAINTAG 0
#define RESPONSE_FINDTRAINVAL 1
#define RESPONSE_FINDWEATHERTAG 2
#define RESPONSE_FINDWEATHERVAL 3
#define RESPONSE_BUFFSZ 512
#define RESPONSE_BUFFRD 64
#define RESPONSE_RETRYMAX 50
char responseBuff[RESPONSE_BUFFSZ + 1] = {0};
String responseCurrent;
//0=origin, 1=destination
int responseLocation;

#define BUTTON_INTTIME_MIN 250
int timeInterval = 50;
unsigned long elapsedTime;
unsigned long startUpTime;
String updateIntervalsString = "60,60";
int updateIntervals[2] = {60, 60};
unsigned long lastChangeTime;
unsigned long noChangeTimeout = 60000;

int8_t timeZone = 0;
int8_t minutesTimeZone = 0;
time_t currentTime;

WiFiClient client;
HTTPClient http;
TFT_eSPI tft = TFT_eSPI();

//OFF= on all time with wifi. LIGHT=onll time no wifi, DEEP=one shot + wake up
#define SLEEP_MODE_OFF 0
#define SLEEP_MODE_DEEP 1
#define SLEEP_MODE_LIGHT 2

//general variables
int logging = 0;
int sleepMode = SLEEP_MODE_DEEP;
int sleepForce = 0;
float newTemp;

float ADC_CAL =0.96;
float battery_mult = 1220.0/220.0/1024;//resistor divider, vref, max count
float battery_volts;

//datanode 0=trains, 1=weather
int dataMode = 0;
#define MAX_ROWS 40
#define LOCATIONS_MAX 4
//train data
#define S_STD 0
#define S_ETD 1
#define S_PLATFORM 2
#define S_ORIGIN 3
#define S_DESTINATION 4
#define S_HEIGHT 5
#define S_MAX 6

#define WEATHER_TIME "dt_txt"
#define WEATHER_TEMP "temp"
#define WEATHER_MAIN "main"
#define WEATHER_DESCRIPTION "description"
#define WEATHER_RAIN "3h"
#define WEATHER_CLOUD "all"
#define WEATHER_WIND "speed"
#define WEATHER_ICON "icon"

//weather data
#define W_TIME 0
#define W_TEMP 1
#define W_MAIN 2
#define W_DESCRIPTION 3
#define W_RAIN 4
#define W_CLOUD 5
#define W_WIND 6
#define W_ICON 7
#define W_DT 8
#define C_MAX 9

#define WGRID_ROWS 2
#define WGRID_COLS 4
#define WGRID_THEIGHT 17
#define WGRID_ISIZE 40

String dataAccessToken[2] = {"",""};
String dataURL[2] = {"",""};
String dataQuery[2] = {"",""};
String dataFingerprint[2] = {"",""};
String dataLocations[2][LOCATIONS_MAX] = {"","","","","","","",""};
String dataLocationsD[2][LOCATIONS_MAX] = {"","","","","","","",""};
String dataRows = "5";

String weekDays = "SunMonTueWedThuFriSat";
#define BASE_DAY 4
String weatherCitiesString = "";
String weatherCities[LOCATIONS_MAX] = {"","","",""};
String weatherCityNamesString = "";
String weatherCityNames[LOCATIONS_MAX] = {"","","",""};

int locationIndex = 0;
int dataCount;
int olddataCount;
int dataChanged;
int dataRefresh = 1;
int dataOffset = 0;
int setMode = 0;
String dataFields[MAX_ROWS][C_MAX];
int tagPresent[C_MAX] ={0,0,0,0,0,0,0,0};

int displayRows = 11;
int colWidths[S_MAX];
int fieldWidths[S_MAX];
String colHdrs[S_MAX] = {"STD","ETD","PL","From","To"};
int rotation = 1;

void parseCSV(String csv, int fMax, int fType) {
	int i,j,k;
	i=0;
	k = 0;
	while(k < fMax) {
		j = csv.indexOf(',', i);
		if(j<0) j = 255;
		switch(fType) {
			case 0: 
				colWidths[k] = csv.substring(i,j).toInt();
				break;
			case 1:
				fieldWidths[k] = csv.substring(i,j).toInt();
				break;
			case 2:
				dataLocations[0][k] = csv.substring(i,j);
				break;
			case 3:
				dataLocationsD[0][k] = csv.substring(i,j);
				break;
			case 4:
				dataLocations[1][k] = csv.substring(i,j);
				break;
			case 5:
				weatherCityNames[k] = csv.substring(i,j);
				break;
			case 6:
				updateIntervals[k] = csv.substring(i,j).toInt();
				if(updateIntervals[k] < 15) updateIntervals[k] = 15;
				break;
		}
		k++;
		if(j == 255) break;
		i = j + 1;
	}
	//Fill up any unspecified array elements
	switch(fType) {
		case 0: 
			for(i = k; i < fMax; i++) colWidths[k] = 20;
			break;
		case 1:
			for(i = k; i < fMax; i++) fieldWidths[k] = 8;
			break;
		case 2:
			for(i = k; i < fMax; i++) dataLocations[0][i] = dataLocations[0][i-1];
			break;
		case 3:
			for(i = k; i < fMax; i++) dataLocationsD[0][i] = dataLocationsD[0][i-1];
			break;
		case 4:
			for(i = k; i < fMax; i++) dataLocations[1][i] = dataLocations[1][i-1];
			break;
		case 5:
			for(i = k; i < fMax; i++) weatherCityNames[i] = weatherCityNames[i-1];
			break;
		case 6:
			for(i = k; i < fMax; i++) updateIntervals[i] = updateIntervals[i-1];
			break;
	}
}

/*
  load config
*/
void loadConfig() {
	String colWidthsHtString = "44,66,20,100,100,20";
	String fieldWidthsString = "5,8,2,12,12";
	String dataLocationsString[2] = {"",""};
	String dataLocationsDString[2] = {"",""};
	String line = "";
	int config = 0;
	File f = FILESYS.open(CONFIG_FILE, "r");
	if(f) {
		Serial.println("load config opened");delay(10);
		while(f.available()) {
			line =f.readStringUntil('\n');
			line.replace("\r","");
			if(line.length() > 0 && line.charAt(0) != '#') {
				switch(config) {
					case 0: host = line;break;
					case 1: dataAccessToken[0] = line;break;
					case 2: dataURL[0] = line;break;
					case 3: dataQuery[0] = line;break;
					case 4: dataFingerprint[0] = line;break;
					case 5: dataLocationsString[0] = line;break;
					case 6: dataLocationsDString[0] = line;break;
					case 7: dataRows = line;break;
					case 8: dataAccessToken[1] = line;break;
					case 9: dataURL[1] = line;break;
					case 10: dataQuery[1] = line;break;
					case 11: dataFingerprint[1] = line;break;
					case 12: dataLocationsString[1] = line;break;
					case 13: sleepMode = line.toInt();break;
					case 14: updateIntervalsString = line;break;
					case 15: noChangeTimeout = line.toInt();break;
					case 16: colWidthsHtString = line; break;
					case 17: fieldWidthsString = line; break;
					case 18: rotation = line.toInt();break;
					case 19: displayRows = line.toInt();break;
					case 20: weatherCityNamesString = line;break;
					case 21:
						ADC_CAL =line.toFloat();
						Serial.println(F("Config loaded from file OK"));
						break;
				}
				config++;
			}
		}
		f.close();
		Serial.println("Config loaded");delay(10);
		if(updateIntervalsString == "") updateIntervalsString = "60,60";
		if(dataRows.toInt() > MAX_ROWS) dataRows = String(MAX_ROWS);
		if(displayRows < 4) displayRows = 4;
		Serial.print(F("host:"));Serial.println(host);
		Serial.print(F("trainsAccessToken:"));Serial.println(dataAccessToken[0]);
		Serial.print(F("trainsURL:"));Serial.println(dataURL[0]);
		Serial.print(F("trainsQuery:"));Serial.println(dataQuery[0]);
		Serial.print(F("trainsFingerprint:"));Serial.println(dataFingerprint[0]);
		Serial.print(F("trainStationsString:"));Serial.println(dataLocationsString[0]);
		Serial.print(F("trainDestinationsString:"));Serial.println(dataLocationsDString[0]);
		Serial.print(F("dataRows:"));Serial.println(dataRows);
		Serial.print(F("weatherAccessToken:"));Serial.println(dataAccessToken[1]);
		Serial.print(F("weatherURL:"));Serial.println(dataURL[1]);
		Serial.print(F("weatherQuery:"));Serial.println(dataQuery[1]);
		Serial.print(F("weatherFingerprint:"));Serial.println(dataFingerprint[1]);
		Serial.print(F("weatherCitiesString:"));Serial.println(dataLocationsString[1]);
		Serial.print(F("sleepMode:"));Serial.println(sleepMode);
		Serial.print(F("updateIntervals:"));Serial.println(updateIntervalsString);
		Serial.print(F("noChangeTimeout:"));Serial.println(noChangeTimeout);
		Serial.print(F("colWidthsHtString:"));Serial.println(colWidthsHtString);
		Serial.print(F("fieldWidthsString:"));Serial.println(fieldWidthsString);
		Serial.print(F("rotation:"));Serial.println(rotation);
		Serial.print(F("displayRows:"));Serial.println(displayRows);
		Serial.print(F("weatherCityNames:"));Serial.println(weatherCityNamesString);
		Serial.print(F("ADC_CAL:"));Serial.println(ADC_CAL);
		parseCSV(colWidthsHtString, S_MAX, 0);
		parseCSV(fieldWidthsString, S_MAX - 1, 1);
		parseCSV(dataLocationsString[0], LOCATIONS_MAX, 2);
		parseCSV(dataLocationsDString[0], LOCATIONS_MAX, 3);
		parseCSV(dataLocationsString[1], LOCATIONS_MAX, 4);
		parseCSV(weatherCityNamesString, LOCATIONS_MAX, 5);
		parseCSV(updateIntervalsString, 2, 6);
	} else {
		Serial.println(String(CONFIG_FILE) + " not found");
	}
}

/*
   clean missing fields
*/
void cleanFields() {
	int i;
	
	for(i = 0; i < C_MAX; i++) {
		if(tagPresent[i] == 0) {
			dataFields[dataCount-1][i] = "";
		}
		tagPresent[i] = 0;
	}
}

/*
   process xml tag, val pair
*/
void processTag(String tagName, String tagVal) {
	String tag;
	int index;
	int iTag;
	
	//Serial.println("t:" + tagName + " v:" + tagVal);
	iTag = -1;
	index = tagName.indexOf(':');
	if(index >=0) {
		tag = tagName.substring(index+1);
	} else {
		tag = tagName;
	}
	if (dataMode == 0) {
		if(tag == SERVICE_START) {
			if(dataCount) cleanFields();
			dataCount++;
		} else if(tag == SERVICE_TIME) {
				iTag = S_STD;
		} else if(tag == SERVICE_EXPECTED) {
			iTag = S_ETD;
		} else if(tag == SERVICE_PLATFORM) {
			iTag = S_PLATFORM;
		} else if(tag == SERVICE_ORIGIN) {
			responseLocation = 0;
		} else if(tag == SERVICE_DESTINATION) {
			responseLocation = 1;
		} else if(tag == SERVICE_LOCATION) {
			if(responseLocation) {
				iTag = S_DESTINATION;
			} else {
				iTag = S_ORIGIN;
			}
		}
	} else {
		if(tag == WEATHER_DT) {
			if(dataCount) cleanFields();
			iTag = W_DT;
			dataCount++;
		} else if(tag == WEATHER_TIME) {
			iTag = W_TIME;
		} else if(tag == WEATHER_TEMP) {
			iTag = W_TEMP;
		} else if(tag == WEATHER_MAIN) {
			iTag = W_MAIN;
		} else if(tag == WEATHER_DESCRIPTION) {
			iTag = W_DESCRIPTION;
		} else if(tag == WEATHER_RAIN) {
			iTag = W_RAIN;
		} else if(tag == WEATHER_WIND) {
			iTag = W_WIND;
		} else if(tag == WEATHER_CLOUD) {
			iTag = W_CLOUD;
		} else if(tag == WEATHER_ICON) {
			iTag = W_ICON;
		}
	}
	if(dataCount && iTag >= 0) {
		tagPresent[iTag] = 1;
		if(dataFields[dataCount-1][iTag] != tagVal) {
			dataFields[dataCount-1][iTag] = tagVal;
			dataChanged = 1;
		}
	}
}

/*
   process responseBuff
*/
void processResponseBuff() {
	int responseState = dataMode * 2 + RESPONSE_FINDTRAINTAG;
	char *p1;
	char *p2;
	char *p3;
	char t;
	String tagName, tagVal;
	
	p1 = responseBuff;
	//Serial.println("process:" + String(p1));
	while(p1 && strlen(p1)) {
		switch(responseState) {
			case RESPONSE_FINDTRAINTAG :
				p1 = strchr(p1, '<');
				if(p1) {
					p2 = strchr(p1, '>');
					if(p2) {
						p2[0] = 0;
						tagName = String(p1+1);
						p2[0] = '>';
						if(tagName.indexOf('/') < 0) {
							responseState = RESPONSE_FINDTRAINVAL;
							p1 = p2 + 1;
						} else {
							memmove(responseBuff, p2 + 1, strlen(p2) + 1);
							p1 = responseBuff;
						}
					} else {
						p1 = NULL;
					}
				}
				break;
			case RESPONSE_FINDTRAINVAL :
				p2 = strchr(p1, '<');
				if(p2) {
					p2[0] = 0;
					tagVal = String(p1);
					p2[0] = '<';
					processTag(tagName, tagVal);
					memmove(responseBuff, p2, strlen(p2) + 1);
					p1 = responseBuff;
					responseState = RESPONSE_FINDTRAINTAG;
				} else {
					p1 = NULL;
				}
				break;
			case RESPONSE_FINDWEATHERTAG :
				p2 = strstr(p1, "\":");
				if(p2) {
					p2[0] = 0;
					p1 = strrchr(p1, '"');
					if(p1) {
						tagName = String(p1+1);
						responseState = RESPONSE_FINDWEATHERVAL;
						p1 = p2 + 2;
					} else {
						p1 = NULL;
					}
					p2[0] = '"';
				} else {
					p1 = NULL;
				}
				break;
			case RESPONSE_FINDWEATHERVAL :
				p2 = strchr(p1, ',');
				p3 = strchr(p1, '}');
				if(p2 && p3  && p3 < p2) p2 = p3;
				if(p2) {
					t = p2[0];
					p2[0] = 0;
					tagVal = String(p1);
					p2[0] = t;
					tagVal.trim();
					if(tagVal.charAt(0) != '{' && tagVal.charAt(0) != '['){
						tagVal.replace("\"", "");
						processTag(tagName, tagVal);
						//Serial.println("move:" + String(p2));
						memmove(responseBuff, p2, strlen(p2) + 1);
						p1 = responseBuff;
					}
					responseState = RESPONSE_FINDWEATHERTAG;
				} else {
					p1 = NULL;
				}
				break;
		}
	}
	//Serial.println("processed:" + String(responseBuff));
}

/*
 Query on line database and process response
*/
void queryDB(String url, String query) {
	int len,c;
	size_t size, space;
	int httpCode;
	int retry = 0;
	
	Serial.print("[HTTPS] begin...\r\n");
	std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
	if(dataFingerprint[dataMode].length() > 40) {
		client->setFingerprint(dataFingerprint[dataMode].c_str());
	} else {
		client->setInsecure();
	}
	//Serial.println("query:" + query);
	http.begin(*client, url);
	http.addHeader("Content-Type", "text/xml");
	if(query == "null") {
		httpCode = http.GET();
	} else {
		httpCode = http.POST(query);
	}
	if (httpCode > 0) {
		len = http.getSize();
		Serial.printf("[HTTP] code: %d  size:%d\r\n", httpCode, len);
		// file found at server
		if(httpCode >= 200 && httpCode <= 299) {
			// read all data from server
			char *p;
			responseBuff[0] = 0;
			while(http.connected() && (len > 0 || len == -1)) {
				p = responseBuff + strlen(responseBuff);
				size = client->available();
				space = responseBuff - p + RESPONSE_BUFFSZ - 2;
				if (size && (responseBuff - p + RESPONSE_BUFFSZ) > (RESPONSE_BUFFRD - 1)) {
					retry = 0;
					c = client->readBytes((uint8_t*)p, ((size > space) ? space : size));
					if (!c) {
						Serial.println("read timeout");
					}
					p[c] = 0;
					processResponseBuff();
					if (len > 0) {
						len -= c;
					}
				} else {
					delay(50);
					retry++;
					Serial.println("Read response retry:" + String(retry));
					if(retry > RESPONSE_RETRYMAX) {
						break;
					}
				}
				delay(1);
			}
		} 
	} else {
		Serial.print("[HTTP] ... failed, error:" + http.errorToString(httpCode) + "\n");
	}
	Serial.println("http ended");
	http.end();
}

String translate(String msg) {
	msg.replace("%t", dataAccessToken[dataMode]);
	msg.replace("%s", dataLocations[dataMode][locationIndex]);
	msg.replace("%d", dataLocationsD[dataMode][locationIndex]);
	msg.replace("%r", dataRows);
	return msg;
}

void getData() {
	int i;
	dataChanged = 0;
	dataCount = 0;
	responseCurrent = "";
	queryDB(translate(dataURL[dataMode]), translate(dataQuery[dataMode]));
	Serial.println("Query dataCount:" + String(dataCount));
	if(dataCount) cleanFields();
	if(dataCount != olddataCount) {
		dataChanged = 1;
		olddataCount = dataCount;
	}
}

void initDisplay(int first) {
	int i, j;
	int x, x1;
	int y;
	String hdr;
	Serial.println("displayInit " + String(first) + " " + String(dataMode));
	if(dataMode == 0) {
		tft.fillRect(0, 0, TFT_HEIGHT, colWidths[S_HEIGHT], TFT_GREEN);
		tft.setTextColor(TFT_BLACK, TFT_GREEN);
		x = 0;
		x1 = 0;
		y = 0;
		for(i = 0; i < S_MAX; i++) {
			x1 = x + colWidths[i] / 2;
			x+= colWidths[i];
			hdr = colHdrs[i];
			if(i == 3) hdr+= " - " + dataLocations[0][locationIndex];
			if(i == 4) hdr+= " - " + dataLocationsD[0][locationIndex];
			tft.drawCentreString(hdr, x1,y,2);
		}
		if(first) {
			y += colWidths[S_HEIGHT];
			for(i=0; i < displayRows; i++) {
				tft.fillRect(0, y, TFT_HEIGHT, colWidths[S_HEIGHT],  (i & 1)? TFT_BLUE : TFT_RED);
				y+= colWidths[S_HEIGHT];
			}
			tft.setTextColor(TFT_WHITE, TFT_RED);
			tft.drawCentreString("Wait", colWidths[0] / 2, colWidths[S_HEIGHT],2);
		}
	} else {
		tft.fillRect(0, 0, TFT_HEIGHT, TFT_WIDTH, TFT_BLUE);
		tft.setTextColor(TFT_WHITE, TFT_BLUE);
		tft.drawCentreString("Weather for " + weatherCityNames[locationIndex], TFT_HEIGHT / 2, TFT_WIDTH / 2, 2);
	}
}

void displayWeatherCell(int index, int x, int y) {
	int startX = x * TFT_HEIGHT / WGRID_COLS;
	int startY = y * TFT_WIDTH / WGRID_ROWS;
	int centreCell = startX + TFT_HEIGHT / WGRID_COLS / 2;
	String temp;
	long weekDay;
	int icon;
	
	tft.setTextColor(TFT_WHITE, TFT_BLUE);
	tft.fillRect(startX, startY, TFT_HEIGHT / WGRID_COLS, TFT_WIDTH / WGRID_ROWS, TFT_BLUE);
	tft.drawRect(startX, startY, TFT_HEIGHT / WGRID_COLS, TFT_WIDTH / WGRID_ROWS, TFT_WHITE);
	weekDay = ((dataFields[index][W_DT].toInt() / 86400 + BASE_DAY) % 7) * 3;
	startY += 1;
	tft.drawCentreString(weekDays.substring(weekDay, weekDay + 3) + "-" + dataFields[index][W_TIME].substring(11,16), centreCell, startY, 2);
	startY += WGRID_THEIGHT;
	icon = iconNames.indexOf(dataFields[index][W_ICON]) / 3;
	if(icon >= 0) {
		tft.pushImage(centreCell - ICON_WIDTH / 2, startY, ICON_WIDTH, ICON_HEIGHT, weatherIcons[icon], 0xffff);
	}
	startY += ICON_HEIGHT + 1;
	tft.drawCentreString(dataFields[index][W_MAIN], centreCell, startY, 2);
	startY += WGRID_THEIGHT;
	temp = dataFields[index][W_TEMP];
	if(temp.length() > 0) {
		temp = String(int(round(temp.toFloat() - ABS_ZERO))) + "C";
	} else{
		temp = "**";
	}
	tft.drawCentreString(temp, centreCell, startY, 2);
	startY += WGRID_THEIGHT;
	temp = String(dataFields[index][W_RAIN].toFloat()) + "mm";
	if(temp == "0.00mm") temp = "Dry";
	tft.drawCentreString(temp, centreCell, startY, 2);
	startY += WGRID_THEIGHT;
	temp = dataFields[index][W_WIND];
	if(temp.length() > 0) {
		temp = String(int(round(temp.toFloat() * 3.6))) + "km/hr";
	} else{
		temp = "**";
	}
	tft.drawCentreString(temp, centreCell, startY, 2);
}

void displayData(int start) {
	int i, j;
	int x, x1;
	int y;
	
	if(dataChanged) {
		Serial.println("displayData:" + String(dataCount) + " start:" + String(start));
		if(dataMode == 0) {
			y = colWidths[S_HEIGHT];
			for(i=0; i < displayRows; i++) {
				tft.fillRect(0, y, TFT_HEIGHT, colWidths[S_HEIGHT],  (i & 1)? TFT_BLUE : TFT_RED);
				if((i + start) < dataCount) {
					tft.setTextColor(TFT_WHITE, (i & 1)? TFT_BLUE : TFT_RED);
					x = 0;
					x1 = 0;
					for(j = 0; j < C_MAX; j++) {
						if(j < S_MAX) {
							x1 = x + colWidths[j] / 2;
							x+= colWidths[j];
							tft.drawCentreString(dataFields[i + start][j].substring(0,fieldWidths[j]),x1,y,2);
						}
						//Serial.print(dataFields[i + start][j] + ",");
					}
				}
				//Serial.println();
				y+= colWidths[S_HEIGHT];
			}
			x = 0;
			for (i = 0; i < S_MAX; i++) {
				x+= colWidths[i];
				tft.drawLine(x - 1, 0, x - 1, TFT_WIDTH - 1, TFT_WHITE);
			}
		} else {
			//Weather display
			//rows
			for(y=0; y < WGRID_ROWS; y++) {
				//columns
				for(x=0; x < WGRID_COLS; x++) {
					displayWeatherCell(start + y * WGRID_COLS + x, x, y );
				}
			}
		}
	}
}

/*
	check and time button events
*/
int checkButtons() {
	int i;
	int pin;
	unsigned long period;
	int changed = 0;
	
	for(i=0; i<3;i++) {
		pin = digitalRead(pinInputs[i]);
		if(pin == 0 && pinStates[i] == 1){
			pinTimes[i] = elapsedTime;
		} else if(pin == 1 && (pinStates[i] == 0)) {
			changed = 1;
			period = (elapsedTime - pinTimes[i]) * timeInterval;
			pinChanges[i] = (period>LONG_PRESS) ? 2:1;
		}
		pinStates[i] = pin;
	}
	return changed;
}

/*
	action button events
*/
int processButtons() {
	int changed = 0;
	int rows = dataMode ? WGRID_COLS * WGRID_ROWS : displayRows;
	if(pinChanges[KEY1] == 2) {
		//Long press
		locationIndex++;
		if(locationIndex >= LOCATIONS_MAX) locationIndex = 0;
		initDisplay(0);
		changed = 1;
		pinChanges[KEY1] = 0;
	} else if (pinChanges[KEY1] == 1) {
		//Short press
		dataOffset -= rows;
		if(dataOffset < 0) dataOffset = 0;
		changed = 1;
		dataChanged = 1;
		dataRefresh = 0;
		pinChanges[KEY1] = 0;
	} else if (pinChanges[KEY2] == 2) {
		//Long press
		sleepForce = 1;
		changed = 2;
		dataRefresh = 0;
		pinChanges[KEY2] = 0;
	} else if (pinChanges[KEY2] == 1) {
		//Short press
		dataMode++;
		if(dataMode > 1) dataMode = 0;
		Serial.println("dataMode:" + String(dataMode));
		initDisplay(0);
		dataOffset = 0;
		changed = 1;
		pinChanges[KEY2] = 0;
	} else if (pinChanges[KEY3] == 2) {
		//Long press
		if(dataMode == 0) {
			tft.fillRect(0, colWidths[S_HEIGHT], TFT_HEIGHT, colWidths[S_HEIGHT], TFT_RED);
			tft.drawString("Battery:" + String(battery_volts) + " ip:" + WiFi.localIP().toString(),0,colWidths[S_HEIGHT],2);
			changed = 1;
			dataRefresh = 0;
			//prevent overwrite of display
			dataChanged = 0;
		}
		pinChanges[KEY3] = 0;
	} else if (pinChanges[KEY3] == 1) {
		//Short press
		dataOffset += rows;
		if(dataOffset > (dataCount - rows)) dataOffset = dataCount - rows;
		changed = 1;
		dataChanged = 1;
		dataRefresh = 0;
		pinChanges[KEY3] = 0;
	}
	Serial.println("offset:" + String(dataOffset));
	return changed;
}

void setupStart() {
	startUpTime = millis();
	if(POWER_HOLD_PIN >= 0) {
		digitalWrite(POWER_HOLD_PIN, 0);
		pinMode(POWER_HOLD_PIN, OUTPUT);
	}
	sleepMode = SLEEP_MODE_OFF;
}

void handleGetData() {
	String response;
	int i, j, k;
	
	Serial.println("GetData:" + String(dataMode) + ":" + String(dataCount));
	if(dataMode == 0) {
		response = "Time,Expected,Platform,Origin,Destination<BR>";
		k = 5;
	} else {
		response = "Time,Temp,Main,Description,Rain mm,Cloud %,Windspeed km/hr <BR>";
		k = 7;
	}
	for(i = 0; i < dataCount; i++) {
		for(j = 0; j < k; j++) {
			response += String(dataFields[i][j]);
			if(j < (k-1)) {
				response += ",";
			}
		}
		response += "<BR>";
	}
	lastChangeTime = millis();
    server.send(200, "text/html", response);
}

void handleSetMode() {
	setMode = server.arg("mode").toInt();
	if(setMode != 1 && setMode != 2) setMode = 0;
	if((setMode == 1) && (dataMode == 1) || (setMode == 2) && (dataMode == 0)) {
		pinChanges[KEY2] = 1;
	}
	lastChangeTime = millis();
	Serial.println("SetMode:" + String(setMode));
    server.send(200, "text/html", "Mode Changed:" + String(dataMode));
}

void extraHandlers() {
	Serial.println("Extra handlers");
	server.on("/getData", handleGetData);
	server.on("/setMode", handleSetMode);
}
 
void setupEnd() {
	int i;
	Serial.println("Set up end process");
	tft.init();
	tft.setRotation(rotation);
	initDisplay(1);
	for(i=0; i<3;i++) {
		pinMode(pinInputs[i], INPUT_PULLUP);
		pinStates[i] = 1;
		pinTimers[i] = elapsedTime;
	}
	if(digitalRead(pinInputs[KEY3]) == 0) {
		sleepMode = SLEEP_MODE_OFF;
		Serial.println(F("Sleep override active"));
	}
	lastChangeTime = millis();
}

/*
  Main loop to read temperature and publish as required
*/
void loop() {
	int i,c;
	battery_volts = battery_mult * ADC_CAL * analogRead(A0);
	if(dataRefresh) getData();
	dataRefresh = 1;
	displayData(dataOffset);
	if((sleepMode == SLEEP_MODE_DEEP) && (millis() > (lastChangeTime  + noChangeTimeout)) || sleepForce) {
		Serial.println("sleeping");
		WiFi.mode(WIFI_OFF);
		delaymSec(50);
		WiFi.forceSleepBegin();
		delaymSec(1000);
		if(POWER_HOLD_PIN >=0) pinMode(POWER_HOLD_PIN, INPUT);
		ESP.deepSleep(0);
	}
	for(i = 0;i < updateIntervals[dataMode] * 1000 / timeInterval; i++) {
		server.handleClient();
		wifiConnect(1);
		delaymSec(timeInterval);
		elapsedTime++;
		if(setMode || checkButtons()) {
			setMode = 0;
			c = processButtons();
			if(c) {
				if(c == 1) {
					lastChangeTime = millis();
				}
				break;
			}
		}
	}
	delay(10);
}

