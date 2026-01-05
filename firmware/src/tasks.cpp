#include <Arduino.h>
#include <functional>
#include <vector>

class ScheduledTask {
private:
    struct ScheduledAction {
        std::function<void()> callback;
        uint32_t executeAtMs;
        bool executed;
    };
    
    TaskHandle_t taskHandle = nullptr;
    std::vector<ScheduledAction> scheduledActions;
    SemaphoreHandle_t mutex;
    
    static void updateTask(void* parameter) {
        ScheduledTask* instance = static_cast<ScheduledTask*>(parameter);
        
        while(true) {
            instance->processScheduledActions();
            instance->update();
            vTaskDelay(pdMS_TO_TICKS(10)); // Check every 10ms
        }
    }
    
    void processScheduledActions() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        
        uint32_t now = millis();
        
        // Exécuter les actions dont le temps est venu
        for(auto& action : scheduledActions) {
            if(!action.executed && now >= action.executeAtMs) {
                action.callback();
                action.executed = true;
            }
        }
        
        // Nettoyer les actions exécutées
        scheduledActions.erase(
            std::remove_if(scheduledActions.begin(), scheduledActions.end(),
                [](const ScheduledAction& a) { return a.executed; }),
            scheduledActions.end()
        );
        
        xSemaphoreGive(mutex);
    }
    
protected:
    // Méthode à surcharger pour les mises à jour continues
    virtual void update() {}
    
    // Planifier une action
    void scheduleAction(std::function<void()> callback, uint32_t delayMs) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        
        ScheduledAction action;
        action.callback = callback;
        action.executeAtMs = millis() + delayMs;
        action.executed = false;
        
        scheduledActions.push_back(action);
        
        xSemaphoreGive(mutex);
    }
    
public:
    ScheduledTask() {
        mutex = xSemaphoreCreateMutex();
    }
    
    virtual ~ScheduledTask() {
        if(taskHandle != nullptr) {
            vTaskDelete(taskHandle);
        }
        vSemaphoreDelete(mutex);
    }
    
    void begin(const char* taskName = "ScheduledTask", uint32_t stackSize = 4096, uint8_t priority = 1) {
        xTaskCreate(
            updateTask,
            taskName,
            stackSize,
            this,
            priority,
            &taskHandle
        );
    }
    
    // Annuler toutes les actions planifiées
    void cancelAllScheduled() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        scheduledActions.clear();
        xSemaphoreGive(mutex);
    }
};