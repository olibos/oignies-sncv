#include <Arduino.h>
#include "tasks.cpp"

class Door : public ScheduledTask {
private:
    int pin;
    bool isOpen;
    
    void sequence(){
        digitalWrite(pin, LOW);
        vTaskDelay(pdMS_TO_TICKS(700));
        digitalWrite(pin, HIGH);
    }

    void performOpen() {
        if (isOpen) return;

        isOpen = true;
        sequence();
        Serial.println("Door OPENED");
    }
    
    void performClose() {
        if (!isOpen) return;

        isOpen = false;
        sequence();
        Serial.println("Door CLOSED");
    }
    
public:
    Door(int doorPin) : pin(doorPin), isOpen(false) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }
    
    void open(uint32_t delayMs = 0) {
        if(delayMs == 0) {
            performOpen();
        } else {
            scheduleAction([this]() { this->performOpen(); }, delayMs);
            Serial.printf("Door opening scheduled in %d ms\n", delayMs);
        }
    }
    
    void close(uint32_t delayMs = 0) {
        if(delayMs == 0) {
            performClose();
        } else {
            scheduleAction([this]() { this->performClose(); }, delayMs);
            Serial.printf("Door closing scheduled in %d ms\n", delayMs);
        }
    }
    
    void toggle(uint32_t delayMs = 0) {
        if(isOpen) {
            close(delayMs);
        } else {
            open(delayMs);
        }
    }
    
    bool getState() const { return isOpen; }
};