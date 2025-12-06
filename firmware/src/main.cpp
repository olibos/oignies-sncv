#include <FastLED.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

// UUIDs pour BLE - Personnalisés pour éviter conflits
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CONTROL_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define STATUS_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define STATUS_LED_PIN 2

BLEServer *pServer = NULL;
BLECharacteristic *pControlCharacteristic = NULL;
BLECharacteristic *pStatusCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Configuration des LED
#define LED_PIN 5
#define NUM_LEDS 102
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define BRIGHTNESS 200

CRGB leds[NUM_LEDS];

// Paramètres de l'animation
unsigned long cycleDuration = 60000;
#define BASE_CYCLE_DURATION 60000

// Contrôle de l'animation
enum AnimationMode
{
    MODE_AUTO,
    MODE_DAY,
    MODE_NIGHT,
    MODE_TRANSITION,
    MODE_PAUSED
};

AnimationMode currentMode = MODE_AUTO;
bool animationEnabled = true;
float manualProgress = 0.0;
unsigned long pausedTime = 0;

// Structure pour les étoiles
struct Star
{
    int pos;
    uint8_t brightness;
    uint8_t twinkleSpeed;
    unsigned long lastUpdate;
};

#define NUM_STARS 25
Star stars[NUM_STARS];

unsigned long cycleStart = 0;
unsigned long lastStatusUpdate = 0;

// ===== DÉCLARATIONS FORWARD =====
void setupBLE();
void processCommand(String command);
void sendStatusUpdate();
void setMode(String mode);
void setBrightness(int value);
void setSpeed(int speedPercent);
String getStatusJSON();
void updateAnimation();
void displayDay();
void transitionToNight(float progress);
void transitionToDay(float progress);
void displayStarryNight(unsigned long currentTime);
void shootingStar();

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
    digitalWrite(STATUS_LED_PIN, LOW);

    // Initialisation FastLED
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    // Initialisation des étoiles
    randomSeed(analogRead(0));
    for (int i = 0; i < NUM_STARS; i++)
    {
        stars[i].pos = random(NUM_LEDS);
        stars[i].brightness = random(120, 200);  // Plage plus étroite et plus douce
        stars[i].twinkleSpeed = random(50, 200); // Vitesse de scintillement plus lente
        stars[i].lastUpdate = millis();
    }

    cycleStart = millis();

    // Initialisation BLE
    setupBLE();

    Serial.println("Système prêt !");
    Serial.println("Connectez-vous via BLE");
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
        CONTROL_CHAR_UUID,
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

    // Envoi du statut périodique (toutes les 500ms si connecté)
    if (deviceConnected && millis() - lastStatusUpdate > 500)
    {
        sendStatusUpdate();
        lastStatusUpdate = millis();
    }

    // Animation LED
    if (animationEnabled)
    {
        updateAnimation();
    }

    FastLED.show();
    delay(20);
}

// ===== TRAITEMENT DES COMMANDES =====

void processCommand(String command)
{
    // Parser JSON avec ArduinoJson v7
    if (command.startsWith("{"))
    {
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
            const char *value = doc["value"];
            if (value)
                setMode(String(value));
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
        else if (cmdStr == "STATUS")
        {
            sendStatusUpdate();
        }
        else if (cmdStr == "LED")
        {
            bool value = doc["value"];
            digitalWrite(STATUS_LED_PIN, value ? HIGH : LOW);
        }
    }
    else
    {
        // Format texte simple
        command.toUpperCase();
        if (command == "AUTO")
            setMode("auto");
        else if (command == "DAY")
            setMode("day");
        else if (command == "NIGHT")
            setMode("night");
        else if (command == "PAUSE")
            setMode("pause");
        else if (command == "STOP")
            setMode("stop");
        else if (command == "STATUS")
            sendStatusUpdate();
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
    else if (mode == "night")
    {
        currentMode = MODE_NIGHT;
        animationEnabled = true;
        Serial.println("Mode NUIT");
    }
    else if (mode == "pause")
    {
        currentMode = MODE_PAUSED;
        pausedTime = millis();
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
    unsigned long elapsed = millis() - cycleStart;
    float progress = (float)(elapsed % cycleDuration) / cycleDuration * 100;

    // Remplir le document JSON
    doc["mode"] = modeStr;
    doc["enabled"] = animationEnabled;
    doc["brightness"] = FastLED.getBrightness();
    doc["progress"] = round(progress * 10) / 10.0; // 1 décimale
    doc["speed"] = (BASE_CYCLE_DURATION * 100) / cycleDuration;
    doc["stars"] = NUM_STARS;
    doc["leds"] = NUM_LEDS;

    // Sérialiser en String
    String output;
    serializeJson(doc, output);

    return output;
}

// ===== ANIMATION LED =====

void updateAnimation()
{
    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - cycleStart;

    switch (currentMode)
    {
    case MODE_AUTO:
    {
        if (elapsed >= cycleDuration)
        {
            cycleStart = currentTime;
            elapsed = 0;
        }

        float progress = (float)elapsed / cycleDuration;

        // Cycle complet : Jour → Coucher → Nuit → Lever → Jour
        if (progress < 0.25)
        {
            // Phase 1 : JOUR (0-25%)
            displayDay();
        }
        else if (progress < 0.375)
        {
            // Phase 2 : COUCHER DE SOLEIL (25-37.5%)
            float sunsetProgress = (progress - 0.25) / 0.125;
            transitionToNight(sunsetProgress);
        }
        else if (progress < 0.625)
        {
            // Phase 3 : NUIT ÉTOILÉE (37.5-62.5%)
            displayStarryNight(currentTime);
        }
        else if (progress < 0.75)
        {
            // Phase 4 : LEVER DE SOLEIL (62.5-75%)
            float sunriseProgress = (progress - 0.625) / 0.125;
            transitionToDay(sunriseProgress);
        }
        else
        {
            // Phase 5 : JOUR (75-100%)
            displayDay();
        }
        break;
    }

    case MODE_DAY:
        displayDay();
        break;

    case MODE_NIGHT:
        displayStarryNight(currentTime);
        break;

    case MODE_PAUSED:
        break;
    }
}

void displayDay()
{
    CRGB dayColor = CRGB(135, 206, 250);
    fill_solid(leds, NUM_LEDS, dayColor);
}

void transitionToNight(float progress)
{
    CRGB dayColor = CRGB(135, 206, 250);
    CRGB sunsetColor = CRGB(255, 100, 50);
    CRGB nightColor = CRGB(0, 0, 30);

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

    // Apparition progressive des étoiles dans la deuxième moitié
    if (progress > 0.5)
    {
        float starProgress = (progress - 0.5) * 2;
        for (int i = 0; i < NUM_STARS; i++)
        {
            uint8_t starBrightness = stars[i].brightness * starProgress;
            leds[stars[i].pos] = CRGB(starBrightness * 0.9, starBrightness * 0.9, starBrightness);
        }
    }
}

void transitionToDay(float progress)
{
    CRGB nightColor = CRGB(0, 0, 30);
    CRGB sunriseColor = CRGB(255, 150, 80);  // Orange/rose lever de soleil
    CRGB dayColor = CRGB(135, 206, 250);

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

    // Disparition progressive des étoiles dans la première moitié
    if (progress < 0.5)
    {
        float starProgress = 1.0 - (progress * 2);  // Fade out
        for (int i = 0; i < NUM_STARS; i++)
        {
            uint8_t starBrightness = stars[i].brightness * starProgress;
            leds[stars[i].pos] = CRGB(starBrightness * 0.9, starBrightness * 0.9, starBrightness);
        }
    }
}

void displayStarryNight(unsigned long currentTime)
{
    CRGB nightColor = CRGB(0, 0, 30);
    fill_solid(leds, NUM_LEDS, nightColor);

    for (int i = 0; i < NUM_STARS; i++)
    {
        if (currentTime - stars[i].lastUpdate > stars[i].twinkleSpeed)
        {
            stars[i].lastUpdate = currentTime;

            // Variation plus douce et progressive
            int change = random(-15, 15);                                            // Réduit de 30 à 15
            stars[i].brightness = constrain(stars[i].brightness + change, 100, 220); // Plage réduite
        }

        // Appliquer une transition douce avec interpolation
        uint8_t b = stars[i].brightness;
        // Teinte blanche/bleutée très subtile
        leds[stars[i].pos] = CRGB(b * 0.95, b * 0.95, b);
    }

    // Étoiles filantes encore plus rares
    if (random(2000) < 2)
    { // Divisé par 2 la fréquence
        shootingStar();
    }
}

void shootingStar()
{
    int startPos = random(NUM_LEDS - 10);
    int length = random(4, 7); // Traînée plus courte

    // Animation plus fluide avec dégradé
    for (int frame = 0; frame < 8; frame++)
    { // Plus de frames pour fluidité
        for (int i = 0; i < length; i++)
        {
            int pos = startPos + i + (frame / 2); // Avance plus lentement
            if (pos < NUM_LEDS)
            {
                uint8_t brightness = map(i, 0, length, 220, 50); // Dégradé plus doux
                brightness = brightness * (8 - frame) / 8;       // Fade out progressif
                leds[pos] = CRGB(brightness, brightness, brightness * 1.05);
            }
        }
        FastLED.show();
        delay(25); // Animation plus fluide

        // Effacement progressif de la traînée
        for (int i = 0; i < length; i++)
        {
            int pos = startPos + i + (frame / 2);
            if (pos < NUM_LEDS)
            {
                leds[pos] = CRGB(0, 0, 30);
            }
        }
    }
}