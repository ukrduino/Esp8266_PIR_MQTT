#include <PubSubClient.h>
#include <ESP8266WiFi.h>

// Update these with values suitable for your network.
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "192.168.0.112";

const int GREEN_LED = 12;
const int RED_LED = 15;
const int BLUE_LED = 13;
const int SENSOR_PIN = 14;
const int LDR_PIN = 0;
const int BOARD_LED = 2;

WiFiClient espClient;
PubSubClient client(espClient);
long lastSensorRead = 0;
long lastSensorMsg = 0;
long lastLightMsg = 0;
char msg[50];
int LDRLevel = 0;
bool motionDetected = false; //0 - OK , 1 - Alarm
bool sensorEnabled = false;
byte pirState = LOW;             // we start, assuming no motion detected
byte val = LOW;                  // variable for reading the pin status
int sensorStatus = 0;

void setup() {
	pinMode(GREEN_LED, OUTPUT);
	pinMode(RED_LED, OUTPUT);
	pinMode(BLUE_LED, OUTPUT);
	pinMode(BOARD_LED, OUTPUT);
	pinMode(SENSOR_PIN, INPUT);
	Serial.begin(115200);
	setup_wifi();
	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);
	delay(10000); //sensor calibration period
}


void setup_wifi() {

	delay(50);
	// We start by connecting to a WiFi network
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
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
	if (topic_str == "WittyCloud2/MotionSensor") {
		if ((char)payload[0] == '1') {
			sensorEnabled = true;
		}
		if ((char)payload[0] == '0') {
			sensorEnabled = false;
		}
		motionDetected = false;
	}
}

void reconnect() {
	// Loop until we're reconnected
	while (!client.connected()) {
		Serial.print("Attempting MQTT connection...");
		// Attempt to connect
		if (client.connect("WittyCloud2")) {
			Serial.println("connected");
			// Once connected, publish an announcement...
			client.publish("WittyCloud2/status", "WittyCloud2 connected");
			// ... and resubscribe
			client.subscribe("WittyCloud2/MotionSensor");
		}
		else {
			Serial.print("failed, rc=");
			Serial.print(client.state());
			Serial.println(" try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
}

void loop() {
	if (!client.connected()) {
		reconnect();
	}
	client.loop();
	askSensor();
	processSensorStatus();
	showSensorStatus();
	sendLightSensorData();
}

void sendLightSensorData() {
	long now = millis();
	if (now - lastLightMsg > 2000) {
		lastLightMsg = now;
		int sensorRead = analogRead(0); // read the value from the sensor
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
void askSensor() {
	val = digitalRead(SENSOR_PIN);
	if (val == HIGH) {            // check if the input is HIGH
		digitalWrite(BOARD_LED, LOW); // turn LED ON
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
		digitalWrite(BOARD_LED, HIGH); // turn LED OFF
		if (pirState == HIGH) {
			// we have just turned of
			Serial.println("Motion ended!");
			// We only want to print on the output change, not state
			pirState = LOW;
			motionDetected = false;
		}
	}
}

void showSensorStatus() {
	if (sensorStatus == 4) { //sensorEnabled && motionDetected
		digitalWrite(GREEN_LED, LOW);
		digitalWrite(RED_LED, LOW);
		digitalWrite(BLUE_LED, HIGH);
		return;
	}
	if (sensorStatus == 2) {
		digitalWrite(GREEN_LED, LOW);
		digitalWrite(RED_LED, HIGH);
		digitalWrite(BLUE_LED, LOW);
		return;
	}
	if (!sensorEnabled) {
		digitalWrite(GREEN_LED, HIGH);
		digitalWrite(RED_LED, LOW);
		digitalWrite(BLUE_LED, LOW);
		return;
	}
}

void processSensorStatus() {
	long now = millis();
	if (now - lastSensorRead > 1000) {
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
			return;
		}
		if (!sensorEnabled && !motionDetected) {
			publishSensorStatus(1);
			return;
		}
	}
}

void publishSensorStatus(int status) {
		//sensorStatus = status;
		//char array[1];
		//array[0] = char(status);
	if (sensorStatus != status) {//publishes status if it is changed
		sensorStatus = status;
		client.publish("WittyCloud2/status", String(status).c_str());
		Serial.print("Publish status ");
		Serial.print(status);
	}
	else
	{
		long now = millis();
		if (now - lastSensorMsg > 300000) { //publishes status every 5 min even if it is not changed
			lastSensorMsg = now;
			client.publish("WittyCloud2/status", String(status).c_str());
		}
	}
}