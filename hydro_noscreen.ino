

#include <dht11.h>
#include <Esp8266EasyIoTMsg.h>
#include <Esp8266EasyIoTConfig.h>
#include <Esp8266EasyIoT.h>
#include <SPI.h>
#include <DS1302.h>
#include <Wire.h>                //One Wire library
#include <EEPROMex.h>            //Extended Eeprom library
#include <RTClib.h>


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


RTC_DS1307 RTC;                    //Define RTC module


// Variables
int x, y;
int page = 0;
int tankProgState = 0;
int manualRefilState = 0;
float pH;                          //generates the value of pH

int pmem = 0;                      //check which page your on
float Setpoint;                    //holds value for Setpoint
float HysterisMin;
float HysterisPlus;
float SetHysteris;
float FanTemp;
float FanHumid;

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

DateTime now;                 //call current Date and Time


Esp8266EasyIoT esp;

dht11 dht;
#define DHT11PIN 49;
float lastTemp;
float lastHum;
int lastpH;
#define CHILD_ID_HUM 0
#define CHILD_ID_TEMP 1
#define CHILD_ID_PH 3
Esp8266EasyIoTMsg msgHum(CHILD_ID_HUM, V_HUM);
Esp8266EasyIoTMsg msgTemp(CHILD_ID_TEMP, V_TEMP);
Esp8266EasyIoTMsg msgpH(CHILD_ID_PH, V_PH);



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

	InitDHT();
	Serial.begin(9600);
	delay(300);
	Serial.println("Setting up output pins...");
	delay(700);
}


void logicLoop()
{
	ReadDHT();
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
	}

	if (pH >= HysterisMin && pH <= HysterisPlus && pmem == 0)
	{
		digitalWrite(pHMinPin, LOW);
		digitalWrite(pHPlusPin, LOW);
		Serial.println("ph + and - are LOW (hysteria correction)");
	}

	if (pH < HysterisMin && pmem == 0)
	{
		pmem == 1,
			digitalWrite(pHPlusPin, HIGH);
		digitalWrite(pHMinPin, LOW);
		Serial.println("ph + pin is HIGH");
	}

	if (pH >= HysterisMin && pH < Setpoint && pmem == 1)
	{
		digitalWrite(pHPlusPin, HIGH);
		digitalWrite(pHMinPin, LOW);
		Serial.println("ph + pin is HIGH");
	}

	if (pH > HysterisPlus && pmem == 0)
	{
		pmem == 2,
			digitalWrite(pHMinPin, HIGH);
		digitalWrite(pHPlusPin, LOW);
		Serial.println("ph - pin is HIGH");

	}

	if (pH <= HysterisPlus && pH > Setpoint && pmem == 2)
	{
		digitalWrite(pHMinPin, HIGH);
		digitalWrite(pHPlusPin, LOW);
		Serial.println("ph - pin is HIGH");
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

void InitDHT()
{
	pinMode(dht_dpin, OUTPUT);
	digitalWrite(dht_dpin, HIGH);
}

void ReadDHT()
{
	bGlobalErr = 0;
	byte dht_in;
	byte i;
	digitalWrite(dht_dpin, LOW);
	delay(23);
	digitalWrite(dht_dpin, HIGH);
	delayMicroseconds(40);
	pinMode(dht_dpin, INPUT);
	dht_in = digitalRead(dht_dpin);

	if (dht_in)
	{
		bGlobalErr = 1;//dht start condition 1 not met
		return;
	}

	delayMicroseconds(80);
	dht_in = digitalRead(dht_dpin);

	if (!dht_in)
	{
		bGlobalErr = 2;//dht start condition 2 not met
		return;
	}

	delayMicroseconds(80);
	for (i = 0; i<5; i++)
		dht_dat[i] = read_dht_dat();

	pinMode(dht_dpin, OUTPUT);

	digitalWrite(dht_dpin, HIGH);

	byte dht_check_sum =
		dht_dat[0] + dht_dat[1] + dht_dat[2] + dht_dat[3];

	if (dht_dat[4] != dht_check_sum)
	{
		bGlobalErr = 3;
	}
};


byte read_dht_dat()
{
	byte i = 0;
	byte result = 0;
	for (i = 0; i< 8; i++)
	{
		while (digitalRead(dht_dpin) == LOW);
		delayMicroseconds(45);

		if (digitalRead(dht_dpin) == HIGH)
			result |= (1 << (7 - i));

		while (digitalRead(dht_dpin) == HIGH);
	}
	return result;
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
	RTC.begin();
}

void runPump()
{
	now = RTC.now();
	if ((now.hour() % 2 == 1) && (now.minute() == 0 || now.minute() == 1 || now.minute() == 2))
	{
		//Starts at 7am
		if ((now.hour() - 7) >= 0)
		{
			digitalWrite(pumpPin, HIGH);
		}
	}
	else
	{
		digitalWrite(pumpPin, LOW);
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