/**
 * File sendThingSpeak.ino
 * 
 * int sendThingSpeak(float temp, float hum, float baro)
 * 
 * Fungsi: Mengirimkan suhu (field0), kelembaban(field1) dan tekanan udara (baro)
 * ke ThingSpeak
 * 
 * Return Value: TBD
 * 
 * Hak cipta (c) 2016 x.benny.id. 
 * https://x.benny.id
 * 
 * Lisensi Creative Commons Atribusi-NonKomersial-BerbagiSerupa 4.0 Internasional
 */

#define DEC 2
 
int sendThingSpeak (float temp, float hum, float baro) {
  char mqttUserName[] = "NodeMCUWX";

  WiFiClient client;  // Initialize the Wifi client library.
  PubSubClient mqttClient(client); // Initialize the PuBSubClient library.
  const char* server = "mqtt.thingspeak.com"; 

  mqttClient.setServer(server, 1883);   // Set the MQTT broker details.

  // Loop until reconnected.
  while (!mqttClient.connected()) 
  {
    Serial.print("Attempting MQTT connection...");

    // Connect to the MQTT broker
    if (mqttClient.connect(THINGSPEAK_CLIENT_ID,mqttUserName,THINGSPEAK_MQTT_API)) 
    {
      Serial.print("Connected with Client ID:  ");
      Serial.println(String(THINGSPEAK_CLIENT_ID));
    } else 
    {
      Serial.print("failed, rc=");
      // Print to know why the connection failed.
      // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
  
  // Create data string to send to ThingSpeak
  String data = String("field1=" + String(temp, DEC) + "&field2=" + String(hum, DEC) + "&field3=" + String(baro, DEC));
  int length = data.length();
  char msgBuffer[length];
  data.toCharArray(msgBuffer,length+1);
  //Serial.println(msgBuffer);
  
  // Create a topic string and publish data to ThingSpeak channel feed. 
  String topicString ="channels/" + String( THINGSPEAK_CHANNEL_ID ) + "/publish/"+String(THINGSPEAK_CHANNEL_API);
  length=topicString.length();
  char topicBuffer[length];
  topicString.toCharArray(topicBuffer,length+1);
 
  mqttClient.publish( topicBuffer, msgBuffer );

  mqttClient.disconnect();
}
