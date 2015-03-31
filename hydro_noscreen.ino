/*

Author:  Brandon Byrne, modified hydroponics control for "ebb and flow" setup.
Idea and logicloop based on Tom De Bie "Billie"'s hydroponics controller
Copyright (C) 2015  Brandon Byrne

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Originally based on Billie's hydroponics controller: 
https://github.com/BillieBricks/Billie-s-Hydroponic-Controller

Changes include:
-pump control for an ebb and flow system
-modified pH controller
-EasyIoT incorportation to report status of each sensor over the web
-removed LCD screen requirements
-removed unused features, refer to the original github above


*/

#include <Thermistor.h>
#include <dht11.h>
#include <Esp8266EasyIoTMsg.h>
#include <Esp8266EasyIoTConfig.h>
#include <Esp8266EasyIoT.h>
#include <SPI.h>
#include <DS1302.h>
#include <Wire.h>                //One Wire library
#include <EEPROMex.h>            //Extended Eeprom library

//Thermistor variables
Thermistor waterTemp(A0);
int lastWTemp;
int wTemp;

#define dht_dpin 49                //pin for DHT11
int pHPin = A9;                    //pin for pH probe
int pHPlusPin = 45;                //pin for Base pump (relay)
int pHMinPin = 43;                 //pin for Acide pump (relay)
int pumpPin = 47;				   //pin for main pump
const int kCePin = 36;  // Chip Enable
const int kIoPin = 34;  // Input/Output
const int kSclkPin = 38;  // Serial Clock

// Create a DS1302 object.
DS1302 rtc(kCePin, kIoPin, kSclkPin);
Time now = rtc.time();

// Variables
int x, y;
int page = 0;
int tankProgState = 0;
int manualRefilState = 0;
float pH;                          //generates the value of pH
int phUp=2;						   //Always let IoT flip state on startup
int phDown=2;					   //             |

float Setpoint;                    //holds value for Setpoint
float HysterisMin;
float HysterisPlus;
float SetHysteris;
float FanTemp;
float FanHumid;
int flood;
								   // Variables for pulsing the pump
long previousMillis = 0;           //             |
long pinHighTime = 100;            //             |
long pinLowTime = 7500;            //             |
long pinTime = 100;                //             |

int EepromSetpoint = 10;      //location of Setpoint in Eeprom
int EepromSetHysteris = 20;   //location of SetHysteris in Eeprom

Esp8266EasyIoT esp;

dht11 dht;
#define DHT11PIN 49;
float lastTemp;
float lastHum;
int lastpH;
int lastFlood;
int lastpHPluspin;
int lastpHMinpin;
#define CHILD_ID_HUM 0
#define CHILD_ID_TEMP 1
#define CHILD_ID_PH 3
#define CHILD_ID_FLOOD 4
#define CHILD_ID_PH_UP 5
#define CHILD_ID_PH_DOWN 6
#define CHILD_ID_WTEMP 7
Esp8266EasyIoTMsg msgHum(CHILD_ID_HUM, V_HUM);
Esp8266EasyIoTMsg msgTemp(CHILD_ID_TEMP, V_TEMP);
Esp8266EasyIoTMsg msgpH(CHILD_ID_PH, V_PH);
Esp8266EasyIoTMsg msgFlood(CHILD_ID_FLOOD, V_PH);
Esp8266EasyIoTMsg msgPHup(CHILD_ID_PH_UP, V_DOOR);
Esp8266EasyIoTMsg msgPHdown(CHILD_ID_PH_DOWN, V_DOOR);
Esp8266EasyIoTMsg msgWaterTemp(CHILD_ID_WTEMP, V_TEMP);


void setup()
{
	noScreenSetup();
	EepromRead();
	logicSetup();
	IoTsetup();
}

void loop()
{

	logicLoop();
	runPump();
	IoTreport();

}

void EepromRead()
{
	Setpoint = EEPROM.readFloat(EepromSetpoint);
	SetHysteris = EEPROM.readFloat(EepromSetHysteris);
}



void logicSetup()
{

	pinMode(pHPlusPin, OUTPUT);
	pinMode(pHMinPin, OUTPUT);
	pinMode(pumpPin, OUTPUT);

	Serial.begin(9600);
	delay(300);
	Serial.println("Setting up output pins...");
	delay(700);
	Wire.begin();
}


void logicLoop()
{
	float sensorValue = 0;
	sensorValue = analogRead(pHPin);
	pH = phTest();

	HysterisMin = (Setpoint - SetHysteris);
	HysterisPlus = (Setpoint + SetHysteris);

	if (pH == Setpoint)
	{
		unsigned long currentMillis = millis();
		if (currentMillis - previousMillis > pinTime)
		{
			previousMillis = currentMillis;

			digitalWrite(pHMinPin, LOW);
			digitalWrite(pHPlusPin, LOW);
			Serial.println("ph + and - are LOW");
			phDown = 0;
			phUp = 0;
			pinTime = pinLowTime;
		}
	}

	if (pH >= HysterisMin && pH <= HysterisPlus )
	{
		unsigned long currentMillis = millis();
		if (currentMillis - previousMillis > pinTime)
		{
			previousMillis = currentMillis;
			digitalWrite(pHMinPin, LOW);
			digitalWrite(pHPlusPin, LOW);
			Serial.println("ph + and - are LOW (hysteria correction)");
			phUp = 0;
			phDown = 0;
			pinTime = pinLowTime;
		}
	}

	if (pH < HysterisMin)
	{
		unsigned long currentMillis = millis();
		if (currentMillis - previousMillis > pinTime)
		{
			previousMillis = currentMillis;
			digitalWrite(pHPlusPin, HIGH);
			digitalWrite(pHMinPin, LOW);
			Serial.println("ph + pin is HIGH");
			phUp = 1;
			phDown = 0;
			pinTime = pinHighTime;
		}
	}

	if (pH >= HysterisMin && pH < Setpoint)
	{
		unsigned long currentMillis = millis();
		if (currentMillis - previousMillis > pinTime)
		{
			previousMillis = currentMillis;
			digitalWrite(pHPlusPin, HIGH);
			digitalWrite(pHMinPin, LOW);
			Serial.println("ph + pin is HIGH");
			phUp = 1;
			phDown = 0;
			pinTime = pinHighTime;
		}
	}

	if (pH > HysterisPlus)
	{
		unsigned long currentMillis = millis();
		if (currentMillis - previousMillis > pinTime)
		{
			previousMillis = currentMillis;
			digitalWrite(pHMinPin, HIGH);
			digitalWrite(pHPlusPin, LOW);
			Serial.println("ph - pin is HIGH");
			phDown = 1;
			phUp = 0;
			pinTime = pinLowTime;
		}
	}

	if (pH <= HysterisPlus && pH > Setpoint)
	{
		unsigned long currentMillis = millis();
		if (currentMillis - previousMillis > pinTime)
		{
			previousMillis = currentMillis;
			digitalWrite(pHMinPin, HIGH);
			digitalWrite(pHPlusPin, LOW);
			Serial.println("ph - pin is HIGH");
			phUp = 0;
			phDown = 1;
			pinTime = pinLowTime;
		}
	}
	Serial.print("Setpoint = ");
	Serial.println(Setpoint);
	Serial.print("Hysteris = ");
	Serial.println(SetHysteris);
	Serial.print("pH = ");
	Serial.println(pH);

	
	delay(250);
}

void runPump()
{
	now = rtc.time();
	if ((now.hr % 2 == 1) && (now.min == 0 || now.min == 1 || now.min == 2 || now.min == 3))
	{
		//Starts at 7am
		if ((now.hr - 7) >= 0)
		{
			digitalWrite(pumpPin, HIGH);
			flood = 1;
			Serial.println("Flooding: on");
		}
	}
	else
	{
		digitalWrite(pumpPin, LOW);
		flood = 0;
		Serial.println("Flooding: off");
	}

}

void IoTsetup() {
	Serial2.begin(9600);
	Serial2.setTimeout(1000);
	Serial.println("EasyIoTEsp init");

	dht.read(49);

	pinMode(13, OUTPUT);

	esp.begin(NULL, 3, &Serial2, &Serial);
	esp.present(CHILD_ID_HUM, S_HUM);
	esp.present(CHILD_ID_TEMP, S_TEMP);
	esp.present(CHILD_ID_PH, V_PH);
	esp.present(CHILD_ID_FLOOD, V_PH);
	esp.present(CHILD_ID_PH_UP, S_DOOR);
	esp.present(CHILD_ID_PH_DOWN, S_DOOR);
	esp.present(CHILD_ID_WTEMP, S_TEMP);

}

void IoTreport() {
	while (!esp.process());

	delay(100);

	while (!esp.process());
	dht.read(49);
	float temperature = dht.temperature;
	if (isnan(temperature)) {
		Serial.println("Failed reading temperature from DHT");
	}
	else if (temperature != lastTemp)
	{
		lastTemp = temperature;
		esp.send(msgTemp.set(Fahrenheit(temperature), 1));
		Serial.print("T: ");
		Serial.println(temperature);
	}

	float humidity = dht.humidity;
	if (isnan(humidity)) {
		Serial.println("Failed reading humidity from DHT");
	}
	else if (humidity != lastHum)
	{
		lastHum = humidity;
		esp.send(msgHum.set(humidity, 1));
		Serial.print("H: ");
		Serial.println(humidity);
	}
	int pHlvl = phTest() * 10;

	if ((pHlvl - lastpH) > 1 || (lastpH - pHlvl) > 1)
	{
		lastpH = pHlvl;
		esp.send(msgpH.set(phTest(), 2));
		Serial.print("pH: ");
		Serial.println(phTest());
	}

	if (flood != lastFlood)
	{
		lastFlood = flood;
		esp.send(msgFlood.set(flood));
		Serial.print("Flooding: ");
		Serial.println(flood);
	}
	
	if (phUp != lastpHPluspin)
	{
		lastpHPluspin= phUp;
		esp.send(msgPHup.set(phUp));
	}
	if (phDown != lastpHMinpin)
	{
		lastpHMinpin = phDown;
		esp.send(msgPHdown.set(phDown));
	}
	 wTemp = waterTemp.getTemp();
	if ((wTemp - lastWTemp) > 2 || (lastWTemp - wTemp) > 2)
	{
		lastWTemp = wTemp;
		esp.send(msgWaterTemp.set(Fahrenheit(wTemp), 1));
	}

}

float phTest() {
	float sensorValue = 0;
	sensorValue = analogRead(pHPin) * 5.0 / 1024;
	pH = (3.5 * sensorValue);
	return pH;
}

double Fahrenheit(double celsius)
{
	return 1.8 * celsius + 32;
}

void noScreenSetup() {
	EEPROM.writeFloat(EepromSetpoint, 6.5);  // 6.5 pH for tomato growth
	EEPROM.writeFloat(EepromSetHysteris, 1.0);
}