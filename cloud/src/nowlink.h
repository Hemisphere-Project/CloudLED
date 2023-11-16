
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint32_t nowOffset = 0; 


// nowMillis
uint32_t nowMillis() 
{
  return millis() + nowOffset;
}


// nowInit
void nowInit( void (*receiveCallback)(const uint8_t *mac, const uint8_t *data, int len) )
{
    // Set ESP32 in STA mode to begin with
    WiFi.mode(WIFI_STA);

    // Print MAC address
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());

    // Disconnect from WiFi
    WiFi.disconnect();

    // Initialize ESP-NOW
    if (esp_now_init() == ESP_OK)
    {
        Serial.println("ESP-NOW Init Success");
        esp_now_register_recv_cb(receiveCallback);
    }
    else
    {
        Serial.println("ESP-NOW Init Failed");
        delay(3000);
        ESP.restart();
    }

    // Add Broadcast
    esp_now_peer_info_t peerInfo = {};
    memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
    if (!esp_now_is_peer_exist(broadcastAddress)) esp_now_add_peer(&peerInfo);
}


// nowBroadcast
void nowBroadcast(String msg) {
    esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)msg.c_str(), msg.length());
    // if (result == ESP_OK) {
    //     Serial.println("Sent with success");
    // }
    // else {
    //     Serial.println("Error sending the data");
    // }
}


// TIMESTAMP task
// void nowInfoTask(void* pvParameters) { 
//     while (1) {  
//       nowBroadcast("T=" + String(nowMillis()) );
//       delay(1000);
//     }
// }