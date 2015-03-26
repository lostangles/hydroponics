

#include <dht11.h>
#include <Esp8266EasyIoTMsg.h>
#include <Esp8266EasyIoTConfig.h>
#include <Esp8266EasyIoT.h>
#include <SPI.h>
#include <DS1302.h>
#include <Wire.h>                //One Wire library
#include <EEPROMex.h>            //Extended Eeprom library


#define dht_dpin 49                //pin for DHT11
int pHPin = A9;                    //pin for pH probe
int pHPlusPin = 45;                //pin for Base pump (relay)
int pHMinPin = 43;                 //pin for Acide pump (relay)
int ventilatorPin = 47;            //pin for Fan (relay)
int floatLowPin = 7;               //pin for lower float sensor
int floatHighPin = 8;              //pin for upper float sensor
int solenoidPin = 60;              //pin for Solenoid valve (relay)
int lightSensor = 68;              //pin for Photoresistor
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
int phUp;
int phDown;

int pmem = 0;                      //check which page your on
float Setpoint;                    //holds value for Setpoint
float HysterisMin;
float HysterisPlus;
float SetHysteris;
float FanTemp;
float FanHumid;
int flood;

int lightADCReading;
double currentLightInLux;
double lightInputVoltage;
double lightResistance;

int EepromSetpoint = 10;      //location of Setpoint in Eeprom
int EepromSetHysteris = 20;   //location of SetHysteris in Eeprom
int EepromFanTemp = 40;       //location of FanTemp in Eeprom
int EepromFanHumid = 60;      //location of FanHumid in Eeprom


#define DHTTYPE DHT11
byte bGlobalErr;    //for passing error code back.
byte dht_dat[4];    //Array to hold the bytes sent from sensor.


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
Esp8266EasyIoTMsg msgHum(CHILD_ID_HUM, V_HUM);
Esp8266EasyIoTMsg msgTemp(CHILD_ID_TEMP, V_TEMP);
Esp8266EasyIoTMsg msgpH(CHILD_ID_PH, V_PH);
Esp8266EasyIoTMsg msgFlood(CHILD_ID_FLOOD, V_PH);
Esp8266EasyIoTMsg msgPHup(CHILD_ID_PH_UP, V_DOOR);
Esp8266EasyIoTMsg msgPHdown(CHILD_ID_PH_DOWN, V_DOOR);



void setup()
{
	noScreenSetup();
	EepromRead();
	logicSetup();
	timeSetup();
	IoTsetup();
}

void loop()
{

	logicLoop();
	fotoLoop();
	//FanControl();
	runPump();
	TankProgControl();
	IoTreport();

}

void EepromRead()
{
	Setpoint = EEPROM.readFloat(EepromSetpoint);
	SetHysteris = EEPROM.readFloat(EepromSetHysteris);
	FanTemp = EEPROM.read(EepromFanTemp);
	FanHumid = EEPROM.read(EepromFanHumid);
}



void logicSetup()
{

	pinMode(pHPlusPin, OUTPUT);
	pinMode(pHMinPin, OUTPUT);
	pinMode(ventilatorPin, OUTPUT);
	pinMode(solenoidPin, OUTPUT);
	pinMode(pumpPin, OUTPUT);
	pmem == 0;

	Serial.begin(9600);
	delay(300);
	Serial.println("Setting up output pins...");
	delay(700);
}


void logicLoop()
{
	switch (bGlobalErr){
	case 0:
		Serial.print("Light = ");
		Serial.print(dht_dat[0], DEC);
		Serial.println("%  ");
		Serial.print("Temp = ");
		Serial.print(Fahrenheit(dht.temperature));
		Serial.println(" *C  ");

		break;
	case 1:
		Serial.println("Error 1: DHT start condition 1 not met.");
		break;
	case 2:
		Serial.println("Error 2: DHT start condition 2 not met.");
		break;
	case 3:
		Serial.println("Error 3: DHT checksum error.");
		break;
	default:
		Serial.println("Error: Unrecognized code encountered.");
		break;
	}


	float sensorValue = 0;
	sensorValue = analogRead(pHPin);
	pH = phTest();

	HysterisMin = (Setpoint - SetHysteris);
	HysterisPlus = (Setpoint + SetHysteris);

	if (pH == Setpoint)
	{
		pmem == 0,
			digitalWrite(pHMinPin, LOW);
		digitalWrite(pHPlusPin, LOW);
		Serial.println("ph + and - are LOW");
		phDown = 0;
		phUp = 0;
	}

	if (pH >= HysterisMin && pH <= HysterisPlus && pmem == 0)
	{
		digitalWrite(pHMinPin, LOW);
		digitalWrite(pHPlusPin, LOW);
		Serial.println("ph + and - are LOW (hysteria correction)");
		phUp = 0;
		phDown = 0;
	}

	if (pH < HysterisMin && pmem == 0)
	{
		pmem == 1,
			digitalWrite(pHPlusPin, HIGH);
		digitalWrite(pHMinPin, LOW);
		Serial.println("ph + pin is HIGH");
		phUp = 1;
		phDown = 0;
	}

	if (pH >= HysterisMin && pH < Setpoint && pmem == 1)
	{
		digitalWrite(pHPlusPin, HIGH);
		digitalWrite(pHMinPin, LOW);
		Serial.println("ph + pin is HIGH");
		phUp = 1;
		phDown = 0;
	}

	if (pH > HysterisPlus && pmem == 0)
	{
		pmem == 2,
		digitalWrite(pHMinPin, HIGH);
		digitalWrite(pHPlusPin, LOW);
		Serial.println("ph - pin is HIGH");
		phDown = 1;
		phUp = 0;

	}

	if (pH <= HysterisPlus && pH > Setpoint && pmem == 2)
	{
		digitalWrite(pHMinPin, HIGH);
		digitalWrite(pHPlusPin, LOW);
		Serial.println("ph - pin is HIGH");
		phUp = 0;
		phDown = 1;
	}
	Serial.print("pmem = ");
	Serial.println(pmem);
	Serial.print("Setpoint = ");
	Serial.println(Setpoint);
	Serial.print("Hysteris = ");
	Serial.println(SetHysteris);
	Serial.print("pH = ");
	Serial.println(pH);

	
	delay(250);
}

void fotoLoop()
{
		lightADCReading = analogRead(lightSensor);
		// Calculating the voltage of the ADC for light
		lightInputVoltage = 5.0 * ((double)lightADCReading / 1024.0);
		// Calculating the resistance of the photoresistor in the voltage divider
		lightResistance = (10.0 * 5.0) / lightInputVoltage - 10.0;
		// Calculating the intensity of light in lux       
		currentLightInLux = 255.84 * pow(lightResistance, -10 / 9);
}

void FanControl()
{
	if ((dht_dat[0] >= FanHumid) && (dht_dat[2] >= FanTemp))
	{
		digitalWrite(ventilatorPin, HIGH);
	}
	else
	{
		digitalWrite(ventilatorPin, LOW);
	}
}

void TankProgControl()
{
	int levelHigh = LOW;
	int levelLow = LOW;

	levelHigh = digitalRead(floatHighPin);
	levelLow = digitalRead(floatLowPin);

	if (levelHigh == LOW)
	{
		if (levelLow == LOW)
		{
			digitalWrite(solenoidPin, HIGH); //solenoid valve open.
		}
	}
	else
	{
		if (levelLow == HIGH)
		{
			digitalWrite(solenoidPin, LOW); //solenoid valve closed.
		}
	}
}


void ManualRefilProg()
{
	digitalWrite(solenoidPin, HIGH);
}

void timeSetup()
{
	Wire.begin();
	now = rtc.time();
}

void runPump()
{
	now = rtc.time();
	if ((now.hr % 2 == 1) && (now.min == 0 || now.min == 1 || now.min == 2))
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
	EEPROM.writeFloat(EepromSetpoint, 7.0);
	EEPROM.writeFloat(EepromSetHysteris, 1.0);
	EEPROM.writeFloat(EepromFanTemp, 100.0);
	EEPROM.writeFloat(EepromFanHumid, 50.0);
}