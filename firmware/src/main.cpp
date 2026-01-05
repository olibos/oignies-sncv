#include <FastLED.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "wc.cpp"

// UUIDs pour BLE - Personnalisés pour éviter conflits
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define TRAIN_CONTROL_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define STATUS_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define STATUS_LED_PIN 2
#define BEDROOM_PIN 16
#define WC_PIN 17
#define TRAIN_BRAKE 18
#define TRAIN_DIR 19
#define TRAIN_PWM 23
// Paramètres PWM
// Fréquence PWM en Hz (20 kHz recommandé)
#define PWM_FREQ 20000
// Canal PWM
#define PWM_CHANNEL 0
// Résolution 8 bits (0-255)
#define PWM_RESOLUTION 8

BLEServer *pServer = NULL;
BLECharacteristic *pControlCharacteristic = NULL;
BLECharacteristic *pStatusCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Configuration des LED
#define LED_PIN 5
#define NUM_LEDS 105
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define BRIGHTNESS 200

CRGB leds[NUM_LEDS];

// Paramètres de l'animation
uint32_t cycleDuration = 75000;
#define BASE_CYCLE_DURATION 75000

// Contrôle de l'animation
enum AnimationMode
{
    MODE_AUTO,
    MODE_DAY,
    MODE_NIGHT,
    MODE_TRANSITION,
    MODE_PAUSED,
    MODE_MANUAL
};

AnimationMode currentMode = MODE_AUTO;
bool animationEnabled = true;
float manualProgress = 0.0;
Door wc(WC_PIN);
bool forward = true;
uint8_t trainSpeed = 100;

CRGB manualColor;
CRGB dayColor = CRGB(255, 255, 255);
CRGB sunsetColor = CRGB(255, 100, 50);
CRGB nightColor = CRGB(0, 0, 125);

uint32_t cycleStart = 0;
uint32_t lastStatusUpdate = 0;

// ===== DÉCLARATIONS FORWARD =====
void setupBLE();
void processCommand(String command);
void sendStatusUpdate();
void setMode(String mode);
void setBrightness(int value);
void setSpeed(int speedPercent);
void setTrainSpeed(int speedPercent);
void setTrainDirection(bool forward);
String getStatusJSON();
void updateAnimation();
void displayDay();
void transitionToNight(float progress);
void transitionToDay(float progress);
void displayNight();
void move(bool sens, int vitesse);

// ===== CALLBACKS BLE =====

class ServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
        digitalWrite(STATUS_LED_PIN, HIGH); // LED ON quand connecté
        Serial.println("Client BLE connecté");
    }

    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
        digitalWrite(STATUS_LED_PIN, LOW); // LED OFF quand déconnecté
        Serial.println("Client BLE déconnecté");
    }
};

class ControlCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();

        if (value.length() > 0)
        {
            String command = String(value.c_str());
            command.trim();
            Serial.println("Commande BLE reçue: " + command);
            processCommand(command);
        }
    }
};

// ===== SETUP =====

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== Démarrage Module LED ESP32 ===");

    // Initialisation de la LED de statut
    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(BEDROOM_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    digitalWrite(BEDROOM_PIN, HIGH);

    pinMode(TRAIN_DIR, OUTPUT);
    pinMode(TRAIN_BRAKE, OUTPUT);

    // Configuration du PWM
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(TRAIN_PWM, PWM_CHANNEL);

    // Activation du frein au démarrage
    ledcWrite(PWM_CHANNEL, 0);
    digitalWrite(TRAIN_BRAKE, LOW);

    // Initialisation FastLED
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    wc.begin("wc");
    cycleStart = millis();

    // Initialisation BLE
    setupBLE();

    Serial.println("Système prêt !");
    Serial.println("Connectez-vous via BLE");
    delay(100);
    move(forward, trainSpeed);
}

void move(bool sens, int vitesse)
{
    if (sens != forward)
    {
        // 1. Arrêter le PWM
        ledcWrite(PWM_CHANNEL, 0);
        delay(5);

        // 2. Activer le frein
        digitalWrite(TRAIN_BRAKE, HIGH);
        delay(10); // Attendre que le frein soit actif

        // 3. Changer la direction (moteur freiné = sûr)
        digitalWrite(TRAIN_DIR, sens ? LOW : HIGH);
        delay(10); // Attendre la stabilisation

        // 4. Libérer le frein
        digitalWrite(TRAIN_BRAKE, LOW);
        delay(10); // Attendre que le frein soit inactif
        forward = sens;
    }

    trainSpeed = vitesse;
    // 5. Appliquer la vitesse PWM
    ledcWrite(PWM_CHANNEL, vitesse);
}

void setupBLE()
{
    // Get the MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);

    // Create device name with MAC suffix (last 3 bytes)
    char deviceName[32];
    snprintf(deviceName, sizeof(deviceName), "OIGNIES-SNCV-%02X%02X%02X",
             mac[3], mac[4], mac[5]);

    // Créer le device BLE
    BLEDevice::init(deviceName);

    // Créer le serveur BLE
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Créer le service BLE
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Caractéristique de contrôle (WRITE)
    pControlCharacteristic = pService->createCharacteristic(
        TRAIN_CONTROL_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pControlCharacteristic->setCallbacks(new ControlCallbacks());

    // Caractéristique de statut (READ + NOTIFY)
    pStatusCharacteristic = pService->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY);
    pStatusCharacteristic->addDescriptor(new BLE2902());

    // Démarrer le service
    pService->start();

    // Démarrer l'advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // iPhone fix
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLE démarré - En attente de connexion...");
    Serial.print("Nom BLE: ");
    Serial.println(deviceName);
}

// ===== LOOP PRINCIPAL =====
void loop()
{
    // Gestion de la reconnexion BLE
    if (!deviceConnected && oldDeviceConnected)
    {
        delay(500);
        pServer->startAdvertising();
        Serial.println("Redémarrage de l'advertising BLE");
        oldDeviceConnected = false;
    }

    if (deviceConnected && !oldDeviceConnected)
    {
        oldDeviceConnected = true;
    }

    bool update = millis() - lastStatusUpdate > 5000;
    if (update)
    {
        lastStatusUpdate = millis();
    }

    // Envoi du statut périodique (toutes les 500ms si connecté)
    if (deviceConnected && update)
    {
        sendStatusUpdate();
    }

    // Animation LED
    if (animationEnabled)
    {
        updateAnimation();
    }

    leds[104] = leds[70] = leds[69] = leds[35] = leds[34] = leds[0] = CRGB(0, 0, 0);
    FastLED.show();
    delay(20);
}

// ===== TRAITEMENT DES COMMANDES =====
void processCommand(String command)
{
    // Parser JSON avec ArduinoJson v7
    if (!command.startsWith("{"))
        return;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, command);

    if (error)
    {
        Serial.print("Erreur parsing JSON: ");
        Serial.println(error.c_str());
        return;
    }

    const char *cmd = doc["cmd"];

    if (cmd == nullptr)
    {
        Serial.println("Commande manquante");
        return;
    }

    String cmdStr = String(cmd);
    cmdStr.toUpperCase();

    if (cmdStr == "MODE")
    {
        JsonVariant value = doc["value"];
        if (value.is<JsonObject>())
        {
            JsonObject obj = value.as<JsonObject>();
            manualColor = CRGB(obj["r"] | 0, obj["g"] | 0, obj["b"] | 0);
            setMode("manual");
            Serial.print("Color: ");
            Serial.printf("r=%d, g=%d, b=%d\n", manualColor.r, manualColor.g, manualColor.b);
        }
        else if (value.is<const char *>())
        {
            setMode(String(value.as<const char *>()));
        }
    }
    else if (cmdStr == "BRIGHTNESS")
    {
        int value = doc["value"];
        setBrightness(value);
    }
    else if (cmdStr == "SPEED")
    {
        int value = doc["value"];
        setSpeed(value);
    }
    else if (cmdStr == "TRAIN-SPEED")
    {
        int value = doc["value"];
        setTrainSpeed(value);
    }
    else if (cmdStr == "DIRECTION")
    {
        bool forward = doc["value"];
        setTrainDirection(forward);
    }
    else if (cmdStr == "STATUS")
    {
        sendStatusUpdate();
    }
    else if (cmdStr == "LED")
    {
        bool value = doc["value"];
        digitalWrite(STATUS_LED_PIN, value ? HIGH : LOW);
    }
    else if (cmdStr == "WC")
    {
        wc.toggle();
    }
}

void sendStatusUpdate()
{
    if (!deviceConnected)
        return;

    String status = getStatusJSON();
    pStatusCharacteristic->setValue(status.c_str());
    pStatusCharacteristic->notify();
}

// ===== FONCTIONS DE CONTRÔLE =====

void setMode(String mode)
{
    mode.toLowerCase();

    if (mode == "auto")
    {
        currentMode = MODE_AUTO;
        cycleStart = millis();
        animationEnabled = true;
        Serial.println("Mode AUTO");
    }
    else if (mode == "day")
    {
        currentMode = MODE_DAY;
        animationEnabled = true;
        Serial.println("Mode JOUR");
    }
    else if (mode == "manual")
    {
        currentMode = MODE_MANUAL;
        animationEnabled = true;
        Serial.println("Mode MANUEL");
    }
    else if (mode == "night")
    {
        currentMode = MODE_NIGHT;
        animationEnabled = true;
        Serial.println("Mode NUIT");
    }
    else if (mode == "pause")
    {
        currentMode = MODE_PAUSED;
        Serial.println("Mode PAUSE");
    }
    else if (mode == "stop")
    {
        animationEnabled = false;
        FastLED.clear();
        FastLED.show();
        Serial.println("Mode STOP");
    }
}

void setBrightness(int value)
{
    value = constrain(value, 0, 255);
    FastLED.setBrightness(value);
    Serial.println("Luminosité: " + String(value));
}

void setSpeed(int speedPercent)
{
    speedPercent = constrain(speedPercent, 10, 500);
    cycleDuration = (BASE_CYCLE_DURATION * 100) / speedPercent;
    Serial.println("Vitesse: " + String(speedPercent) + "%");
}

void setTrainSpeed(int speedPercent)
{
    speedPercent = constrain(speedPercent, 0, 100);
    Serial.println("Vitesse Train: " + String(speedPercent) + "%");
    move(forward, speedPercent * 2);
}

void setTrainDirection(bool forward)
{
    Serial.println("Vitesse Direction: " + forward ? "avant" : "arrière");
    move(forward, trainSpeed);
}

String getStatusJSON()
{
    JsonDocument doc;

    // Déterminer le mode actuel
    String modeStr = "";
    switch (currentMode)
    {
    case MODE_AUTO:
        modeStr = "auto";
        break;
    case MODE_DAY:
        modeStr = "day";
        break;
    case MODE_NIGHT:
        modeStr = "night";
        break;
    case MODE_PAUSED:
        modeStr = "paused";
        break;
    }

    // Calculer la progression
    uint32_t elapsed = millis() - cycleStart;
    float progress = (float)(elapsed % cycleDuration) / cycleDuration * 100;

    // Remplir le document JSON
    doc["mode"] = modeStr;
    doc["enabled"] = animationEnabled;
    doc["brightness"] = FastLED.getBrightness();
    doc["progress"] = round(progress * 10) / 10.0; // 1 décimale
    doc["speed"] = (BASE_CYCLE_DURATION * 100) / cycleDuration;
    doc["leds"] = NUM_LEDS;

    // Sérialiser en String
    String output;
    serializeJson(doc, output);

    return output;
}

// ===== ANIMATION LED =====

void updateAnimation()
{
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - cycleStart;

    switch (currentMode)
    {
    case MODE_AUTO:
    {
        if (elapsed >= cycleDuration)
        {
            cycleStart = currentTime;
            elapsed = 0;
            wc.open();
            wc.close(5000);
        }

        float progress = (float)elapsed / cycleDuration;

        // Cycle complet : Jour → Coucher → Nuit → Lever → Jour
        if (progress < 0.3)
        {
            // Phase 1 : JOUR (0-30%)
            displayDay();
        }
        else if (progress < 0.4)
        {
            // Phase 2 : COUCHER DE SOLEIL (30-40%)
            float sunsetProgress = (progress - 0.3) / 0.1;
            transitionToNight(sunsetProgress);
            digitalWrite(BEDROOM_PIN, LOW);
        }
        else if (progress < 0.6)
        {
            // Phase 3 : NUIT ÉTOILÉE (40-60%)
            displayNight();
        }
        else if (progress < 0.7)
        {
            // Phase 4 : LEVER DE SOLEIL (60-70%)
            float sunriseProgress = (progress - 0.6) / 0.1;
            transitionToDay(sunriseProgress);
        }
        else
        {
            // Phase 5 : JOUR (70-100%)
            displayDay();
            digitalWrite(BEDROOM_PIN, HIGH);
        }
        break;
    }

    case MODE_DAY:
        displayDay();
        break;

    case MODE_NIGHT:
        displayNight();
        break;

    case MODE_PAUSED:
        break;

    case MODE_MANUAL:
        fill_solid(leds, NUM_LEDS, manualColor);
        break;
    }
}

void displayDay()
{
    fill_solid(leds, NUM_LEDS, dayColor);
}

void transitionToNight(float progress)
{
    CRGB currentColor;

    if (progress < 0.5)
    {
        // Jour → Orange crépusculaire
        float subProgress = progress * 2;
        currentColor = blend(dayColor, sunsetColor, subProgress * 255);
    }
    else
    {
        // Orange crépusculaire → Nuit
        float subProgress = (progress - 0.5) * 2;
        currentColor = blend(sunsetColor, nightColor, subProgress * 255);
    }

    fill_solid(leds, NUM_LEDS, currentColor);
}

void transitionToDay(float progress)
{
    CRGB sunriseColor = CRGB(255, 150, 80); // Orange/rose lever de soleil
    CRGB currentColor;

    if (progress < 0.5)
    {
        // Nuit → Orange lever de soleil
        float subProgress = progress * 2;
        currentColor = blend(nightColor, sunriseColor, subProgress * 255);
    }
    else
    {
        // Orange lever de soleil → Jour
        float subProgress = (progress - 0.5) * 2;
        currentColor = blend(sunriseColor, dayColor, subProgress * 255);
    }

    fill_solid(leds, NUM_LEDS, currentColor);
}

void displayNight()
{
    fill_solid(leds, NUM_LEDS, nightColor);
}