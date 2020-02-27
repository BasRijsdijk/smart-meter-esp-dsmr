#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

#include <dsmr.h>
#include "reader2.h"
// Remove all traces of secrets before publishing to github!
#include "secrests.h"

using P1Data = ParsedData<
  /* String */ identification,
  /* String */ p1_version,
  /* String */ timestamp,
  /* String */ equipment_id,
  /* FixedValue */ energy_delivered_tariff1,
  /* FixedValue */ energy_delivered_tariff2,
  /* FixedValue */ energy_returned_tariff1,
  /* FixedValue */ energy_returned_tariff2,
  /* String */ electricity_tariff,
  /* FixedValue */ power_delivered,
  /* FixedValue */ power_returned,
  /* uint32_t */ electricity_failures,
  /* uint32_t */ electricity_long_failures,
  /* String */ electricity_failure_log,
  /* uint32_t */ electricity_sags_l1,
  /* uint32_t */ electricity_sags_l2,
  /* uint32_t */ electricity_sags_l3,
  /* uint32_t */ electricity_swells_l1,
  /* uint32_t */ electricity_swells_l2,
  /* uint32_t */ electricity_swells_l3,
  /* FixedValue */ voltage_l1,
  /* FixedValue */ voltage_l2,
  /* FixedValue */ voltage_l3,
  /* FixedValue */ current_l1,
  /* FixedValue */ current_l2,
  /* FixedValue */ current_l3,
  /* FixedValue */ power_delivered_l1,
  /* FixedValue */ power_delivered_l2,
  /* FixedValue */ power_delivered_l3,
  /* FixedValue */ power_returned_l1,
  /* FixedValue */ power_returned_l2,
  /* FixedValue */ power_returned_l3,
  /* uint16_t */ gas_device_type,
  /* String */ gas_equipment_id,
  /* TimestampedFixedValue */ gas_delivered
>;


const auto server_ip = IPAddress(192, 168, 2, 8);
WiFiClient wifi_client;
PubSubClient client(server_ip, 1883, wifi_client);
const char* client_id = "dsmr-esp";

/**
 * This illustrates looping over all parsed fields using the
 * ParsedData::applyEach method.
 *
 * When passed an instance of this Printer object, applyEach will loop
 * over each field and call Printer::apply, passing a reference to each
 * field in turn. This passes the actual field object, not the field
 * value, so each call to Printer::apply will have a differently typed
 * parameter.
 *
 * For this reason, Printer::apply is a template, resulting in one
 * distinct apply method for each field used. This allows looking up
 * things like Item::name, which is different for every field type,
 * without having to resort to virtual method calls (which result in
 * extra storage usage). The tradeoff is here that there is more code
 * generated (but due to compiler inlining, it's pretty much the same as
 * if you just manually printed all field names and values (with no
 * cost at all if you don't use the Printer).
 */
struct Printer {
  template<typename Item>
  void apply(Item &i) {
    if (i.present()) {
      String data;
      data += i.val();
      data += Item::unit();
      //Serial.println(data);
      // This is probably an expensive operation.
      // Find out if this can be improved.
      client.publish((String("sensor/dsmr/dsmr-esp/status/") + Item::name).c_str(), data.c_str());

      //Serial.print(Item::name);
      //Serial.print(F(": "));
      //Serial.print(i.val());
      //Serial.print(Item::unit());
      //Serial.println();
    }
  }
};

SoftwareSerial p1meter;
P1Reader2 p1reader(&p1meter);

void setup(){
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  // RX = D2
  // one telegram is ~822 chars long, so 1024 as buffer size should be ok
  p1meter.begin(115200, SWSERIAL_8N1, D2, -1, true, 1024);
  Serial.begin(115200);
  Serial.println("Smart meter reader v0.2.0, by Rick van Schijndel");
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // backoff
  unsigned retry = 0;
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(client_id, username, password)) {
      Serial.println("mqtt connected");
    } else {
      Serial.print("mqtt: failed to connect, rc=");
      Serial.print(client.state());
      // First retry every 5 seconds, but back off to once every 30 seconds after retrying a couple of times.
      unsigned retry_time_s = retry < 5 ? 5 : 30;
      Serial.printf(" retrying in %u seconds\n", retry_time_s);
      // Wait n seconds before retrying
      delay(retry_time_s * 1000);
      retry++;
    }
  }
}

unsigned successful = 0;
unsigned failed = 0;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (p1meter.available()) {
    digitalWrite(LED_BUILTIN, LOW);
    p1reader.loop();
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
  if (p1reader.available()) {
    Serial.println("Data available");
    P1Data data;
    String err;
    if (p1reader.parse(&data, &err)) {
      successful += 1;
      data.applyEach(Printer());
    } else {
      failed += 1;
      Serial.println(err);
    }
    Serial.print("Successful: ");
    Serial.print(successful);
    Serial.print(" failed: ");
    Serial.println(failed);
  }
}

