
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint32_t nowOffset = 0; 
bool nowOK = false;

// nowMillis
uint32_t nowMillis() 
{
    return millis() + nowOffset;
}

// nowAdjust
void nowAdjust(uint32_t remoteMillis) 
{   
    uint32_t localMillis = nowMillis();
    int32_t diff = remoteMillis - localMillis;
    if (diff <= 0) return;

    nowOffset += diff;
    Serial.printf("Time sync: %d - delta %d\n", remoteMillis, diff);
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

    nowOK = true;
}

// nowStop
void nowStop() 
{
    nowOK = false;
    esp_now_unregister_recv_cb();
    esp_now_del_peer(broadcastAddress);
    esp_now_deinit();

    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_STA);
}


// nowBroadcast
void nowBroadcast(String msg) {
    if (!nowOK) return;
    esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)msg.c_str(), msg.length());
    // if (result == ESP_OK) {
    //     Serial.println("Sent with success");
    // }
    // else {
    //     Serial.println("Error sending the data");
    // }
}
