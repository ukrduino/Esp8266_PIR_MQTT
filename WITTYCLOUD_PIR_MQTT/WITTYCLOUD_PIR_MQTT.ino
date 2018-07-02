#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "Credentials.h"


const char* mqtt_server = MQTT_SERVER_IP;
WiFiClient espClient;
unsigned long reconnectionPeriod = 60000;
unsigned long lastBrokerConnectionAttempt = 0;
unsigned long lastWifiConnectionAttempt = 0;
PubSubClient client(espClient);
char msg[50];

//Witty Cloud hardware
const int GREEN_LED = 12;
const int RED_LED = 15;
const int BLUE_LED = 13;
const int SENSOR_PIN = 14;
const int LDR_PIN = 0;
const int BOARD_LED = 2;
int LDRLevel = 0;
int lightTreshold = 30;
long lastLightSensorMsg = 0;
unsigned long lightsOnTime = 0;
int lightsOnPeriod = 300000;
bool darkness = false;
bool lightsOff = true;


//PIR sensor 
bool motionDetected = false; //0 - OK , 1 - Alarm
bool sensorEnabled = false;
bool ledsEnabled = true;
byte pirState = LOW;             // we start, assuming no motion detected
byte val = LOW;                  // variable for reading the pin status
int sensorStatus = 0;
long lastSensorMsg = 0;
int publishSensorStatusPeriod = 60000;
int pirSensorReadPeriod = 1000;
long lastSensorRead = 0;


void setup() {
	pinMode(GREEN_LED, OUTPUT);
	pinMode(RED_LED, OUTPUT);
	pinMode(BLUE_LED, OUTPUT);
	pinMode(BOARD_LED, OUTPUT);
	pinMode(SENSOR_PIN, INPUT);
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);
	setup_wifi();
	delay(10000); //sensor calibration period
}

void connectToBroker() {
	Serial.print("Attempting MQTT connection...");
	// Attempt to connect
	if (client.connect("WittyCloud_2")) {
		Serial.println("connected");
		// Once connected, publish an announcement...
		client.publish("WittyCloud_2/status", "WittyCloud_2 connected");
		client.publish("Weather/update", "1");
		// ... and resubscribe
		client.subscribe("WittyCloud2/motionSensor");
		client.subscribe("WittyCloud2/leds");
		client.subscribe("WittyCloud2/lightTreshold");
		client.subscribe("WittyCloud2/lightsOnPeriod");
	}
	else {
		Serial.print("failed, rc=");
		Serial.print(client.state());
		Serial.println(" try again in 1 minute");
	}
}


void setup_wifi() {

	delay(500);
	// We start by connecting to a WiFi network
	int numberOfNetworks = WiFi.scanNetworks();

	for (int i = 0; i < numberOfNetworks; i++) {
		Serial.print("Network name: ");
		Serial.println(WiFi.SSID(i));
		if (WiFi.SSID(i).equals(SSID_1))
		{
			Serial.print("Connecting to ");
			Serial.println(SSID_1);
			WiFi.begin(SSID_1, PASSWORD_1);
			delay(1000);
			connectToBroker();
			return;
		}
		else if (WiFi.SSID(i).equals(SSID_2))
		{
			Serial.print("Connecting to ");
			Serial.println(SSID_2);
			WiFi.begin(SSID_2, PASSWORD_2);
			delay(1000);
			connectToBroker();
			return;
		}
		else if (WiFi.SSID(i).equals(SSID_3))
		{
			Serial.print("Connecting to ");
			Serial.println(SSID_3);
			WiFi.begin(SSID_3, PASSWORD_3);
			delay(1000);
			connectToBroker();
			return;
		}
		else
		{
			Serial.println("Can't connect to " + WiFi.SSID(i));
		}
	}
}

void callback(char* topic, byte* payload, unsigned int length) {
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	for (int i = 0; i < length; i++) {
		Serial.print((char)payload[i]);
	}
	Serial.println();
	String topic_str = String(topic);
	if (strcmp(topic, "WittyCloud2/motionSensor") == 0 ) {
		if ((char)payload[0] == '1') {
			sensorEnabled = true;
		}
		if ((char)payload[0] == '0') {
			sensorEnabled = false;
		}
		motionDetected = false;
		return;
	}
	if (strcmp(topic, "WittyCloud2/leds") == 0) {
		if ((char)payload[0] == '1') {
			ledsEnabled = true;
		}
		if ((char)payload[0] == '0') {
			ledsEnabled = false;
		}
		return;
	}
	if (strcmp(topic, "WittyCloud2/lightTreshold") == 0) {
		String myString = String((char*)payload);
		lightTreshold = myString.toInt();
		return;
	}
}

void reconnectWifi() {
	long now = millis();
	if (now - lastWifiConnectionAttempt > reconnectionPeriod) {
		lastWifiConnectionAttempt = now;
		setup_wifi();
	}
}

void reconnectToBroker() {
	long now = millis();
	if (now - lastBrokerConnectionAttempt > reconnectionPeriod) {
		lastBrokerConnectionAttempt = now;
		{
			if (WiFi.status() == WL_CONNECTED)
			{
				if (!client.connected()) {
					connectToBroker();
				}
			}
			else
			{
				reconnectWifi();
			}
		}
	}
}


void loop() {
	if (!client.connected()) {
		reconnectToBroker();
	}
	client.loop();
	askPirSensor();
	processSensorAndPublishStatus();
	sendLightSensorData();
	turnLedsOff();
}

void sendLightSensorData() {
	long now = millis();
	if (now - lastLightSensorMsg > 2000) {
		lastLightSensorMsg = now;
		int sensorRead = analogRead(0); // read the value from the sensor
		if (sensorRead < lightTreshold)
		{
			darkness = true;
		}
		else 
		{
			darkness = false;
		}

		if (LDRLevel != sensorRead)
		{
			LDRLevel = sensorRead;
			Serial.print("Publish message light: ");
			Serial.println(LDRLevel);
			client.publish("WittyCloud2/light", String(LDRLevel).c_str());
		}
	}
}

// about motion detection http://henrysbench.capnfatz.com/henrys-bench/arduino-sensors-and-input/arduino-hc-sr501-motion-sensor-tutorial/
void askPirSensor() {
	val = digitalRead(SENSOR_PIN);
	if (val == HIGH) {            // check if the input is HIGH
		if (ledsEnabled)
		{
			digitalWrite(BOARD_LED, LOW); // turn LED ON
		}
		if (pirState == LOW) {
			Serial.println("Motion detected!");
			// We only want to print on the output change, not state
			if (!motionDetected) {
				motionDetected = true;
				return;
			}
			pirState = HIGH;
		}
	}
	else {
		if (ledsEnabled)
		{
			digitalWrite(BOARD_LED, HIGH); // turn LED OFF
		}
		if (pirState == HIGH) {
			// we have just turned of
			Serial.println("Motion ended!");
			// We only want to print on the output change, not state
			pirState = LOW;
			motionDetected = false;
		}
	}
}

void turnLedsOn() {
	if (ledsEnabled && darkness && lightsOff)
	{
		Serial.println("turnLedsOn");
		fadeIn(GREEN_LED);
		fadeIn(RED_LED);
		fadeIn(BLUE_LED);
		lightsOnTime = millis();
		lightsOff = false;
		return;
	}
}

void turnLedsOff() {
	if (!lightsOff)
	{
		long now = millis();
		if (now - lightsOnTime > lightsOnPeriod) {
			Serial.println("turnLedsOff");
			analogWrite(GREEN_LED, 0); // turn LED OFF
			delay(1000);
			analogWrite(RED_LED, 0); // turn LED OFF
			delay(1000);
			analogWrite(BLUE_LED, 0); // turn LED OFF
			delay(1000);
			lightsOff = true;
		}
	}
}

void processSensorAndPublishStatus() {
	long now = millis();
	if (now - lastSensorRead > pirSensorReadPeriod) {
		lastSensorRead = now;
		if (sensorEnabled && motionDetected) {
			publishSensorStatus(4);
			return;
		}
		if (sensorEnabled && !motionDetected) {
			publishSensorStatus(2);
			return;
		}
		if (!sensorEnabled && motionDetected) {
			publishSensorStatus(3);
			turnLedsOn();
			return;
		}
		if (!sensorEnabled && !motionDetected) {
			publishSensorStatus(1);
			return;
		}
	}
}

void publishSensorStatus(int status) {
	long now = millis();
	if (sensorStatus != status) {//publishes status if it is changed
		sensorStatus = status;
		lastSensorMsg = now;
		client.publish("WittyCloud2/sensorStatus", String(status).c_str());
		Serial.println("---------------");
		Serial.print("Publish status ");
		Serial.println(status);
		Serial.println("---------------");
	}
	else
	{
		long now = millis();
		if (now - lastSensorMsg > publishSensorStatusPeriod) { //publishes status every 5 min even if it is not changed
			lastSensorMsg = now;
			client.publish("WittyCloud2/sensorStatus", String(status).c_str());
		}
	}
}

void fadeIn(int ledPin) {
	for (int i = 0; i < 255; i++)
	{
		analogWrite(ledPin, i);
		delay(30);
	}
}