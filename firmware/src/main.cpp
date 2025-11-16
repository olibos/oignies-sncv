#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define LED_PIN 2 // Built-in LED for most ESP32 boards

// UUIDs must match the frontend
#define SERVICE_UUID "1234abcd-0000-0000-0000-000000000000"
#define CHAR_UUID "1234abcd-0000-0000-0000-000000000001"

BLECharacteristic *ledCharacteristic;

class LedCallback : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pChar)
    {
        std::string value = pChar->getValue();
        if (value.length() > 0)
        {
            int state = value[0];
            digitalWrite(LED_PIN, state == 1 ? HIGH : LOW);
        }
    }
};

void setup()
{
    Serial.begin(9600);
    Serial.write("Setup");
    pinMode(LED_PIN, OUTPUT);

    // Get the MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);

    // Create device name with MAC suffix (last 3 bytes)
    char deviceName[32];
    snprintf(deviceName, sizeof(deviceName), "OIGNIES-SNCV-%02X%02X%02X",
             mac[3], mac[4], mac[5]);

    // Initialize BLE with the custom name
    BLEDevice::init(deviceName);
    BLEServer *server = BLEDevice::createServer();

    BLEService *service = server->createService(SERVICE_UUID);

    ledCharacteristic = service->createCharacteristic(
        CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE);

    ledCharacteristic->setCallbacks(new LedCallback());

    service->start();
    server->getAdvertising()->start();
}

void loop()
{
    delay(100);
}
