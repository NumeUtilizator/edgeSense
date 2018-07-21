/* IoT Car
// Measures distance to car in front
// Led Indicator:
// Green - Safe
// Blue - Optimal
// Red - Too Close
 */
/************************* Includes *********************************/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NewPing.h>
#include <ArduinoJson.h>

/************************* Constants *********************************/

// WiFi Access Point Config
#define WLAN_SSID "MB"             //WLAN SSID
#define WLAN_PASS "Marabogdan2016" //WLAN Password

// MQTT Connection Config
const char *mqtt_server = "192.168.1.218"; // MQTT Server IP Address
#define MQTT_PORT 1883                          // MQTT Server port number, use 8883 for SSL

// HC-SR04 Ultrasonic Distance Measuring Module Config
#define TRIGGER_PIN 4    // D2  Pin assignment for HC-SR04 sensor module Trigger
#define ECHO_PIN 5       // D1  Pin assignment for HC-SR04 sensor module Echo
#define MAX_DISTANCE 500 // Max distance (cm) for the module to register
#define PING_DELAY 20    // Time delay befre (re)sending ping

// LED Warning Lights Config
#define RED_PIN 14   // Pin assignment for Red LED
#define GREEN_PIN 13 // Pin assignment for Green LED
#define BLUE_PIN 12  // Pin assignment for Blue LED

/************************* Runtime Parameters *********************************/
double optimalDist = 10;  // optimal distance
double optimalFactor = 1; // optimal distance correction factor
double correctedOptimalDist;
int margin = 4;            // margin for optimal distance
unsigned int distance = 0; //value for measured distance
String distState;
String sID;
char output[256];
String topicInput = "device/";
String topicOutput = "vfdemo/device/";
char pubTopic[256];

/************************* Initalizations *********************************/
//Callback function header

void callback(char *topic, byte *payload, unsigned int length);
void reconnect();
void setColor(int red, int green, int blue);


// Instantiate WiFi Client
WiFiClient espClient;
//WiFiClientSecure client;
PubSubClient client(espClient);

// Instantiate Ultrasonic Distance Measuring Module
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.

/************************* General Setup Routine *********************************/

void setup()
{
    Serial.begin(9600);
    delay(10);

    // Connect to WiFi access point.
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WLAN_SSID);
    WiFi.begin(WLAN_SSID, WLAN_PASS);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    sID = WiFi.macAddress();
    Serial.println("Mac address: ");
    Serial.println(sID);

    //Setup MQTT topics
    topicInput = topicInput + sID;
    topicOutput = topicOutput + sID + "/distance";
    topicOutput = topicOutput;
    Serial.print("Input Topic: ");
    Serial.println(topicInput);
    Serial.print("Output Topic: ");
    Serial.println(topicOutput);
    delay(5000);
    int str_len = topicOutput.length() + 1;
    topicOutput.toCharArray(pubTopic, str_len);
    client.setServer(mqtt_server, MQTT_PORT);
    client.setCallback(callback);

    //initialize LED settings
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);

}

/************************* Main logic in a LOOP ************************/

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

    distance = sonar.ping_cm();
    Serial.print(distance);
    Serial.println(" cm");
    correctedOptimalDist = optimalDist * optimalFactor;
    if ((distance != 0) and (distance < correctedOptimalDist))
    {
        setColor(255, 0, 0); // red
        distState = "Danger";
    }
    else
    {
        if ((distance >= correctedOptimalDist) and (distance <= correctedOptimalDist + margin))
        {
            setColor(0, 255, 0); // blue
            distState = "Optimal";
        }
        else
        {
            if ((distance == 0) or (distance > correctedOptimalDist))
            {
                setColor(0, 0, 255); // green
                distState = "Safe";
            }
        }
    }
    // Construct JSON Data
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &flogoData = jsonBuffer.createObject();
    flogoData["deviceID"] = sID;
    flogoData["distance"] = distance;
    flogoData["distState"] = distState;
    flogoData.printTo(output, sizeof(output));
    // Publish Distance
    Serial.print(F("\nSending Data: "));
    Serial.print(output);
    client.publish(pubTopic, output);
    delay(500);
}

/************************* Functions *********************************/

// Callback for receiving distance factor on MQTT
void devicedatacallback(double x)
{
    Serial.print("Received new optimal distance correction factor: ");
    Serial.println(x);
    optimalFactor = x;
}

void setColor(int red, int green, int blue)
{
    analogWrite(RED_PIN, red);
    analogWrite(GREEN_PIN, green);
    analogWrite(BLUE_PIN, blue);
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    String strFactor = "";
    for (unsigned int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
        strFactor += (char)payload[i];
    }
    Serial.println();
    double x = strFactor.toFloat();
    Serial.print("Received new optimal distance correction factor: ");
    Serial.println(x);
    optimalFactor = x;
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("ESP8266Client"))
        {
            Serial.println("connected");
            int str_len = topicInput.length() + 1;
            char subTopic[str_len];
            topicInput.toCharArray(subTopic, str_len);
            client.subscribe(subTopic);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println("try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

