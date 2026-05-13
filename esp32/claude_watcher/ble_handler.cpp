// esp32/claude_watcher/ble_handler.cpp
#include "ble_handler.h"
#include <NimBLEDevice.h>

#define SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define CHAR_UUID    "12345678-1234-1234-1234-123456789abd"

static BLEStateCallback g_callback = nullptr;

class CharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        std::string raw = pChar->getValue();
        String val(raw.c_str());
        Serial.println("[BLE] " + val);

        if (!g_callback) return;

        State newState = State::IDLE;
        char toolName[32] = "";
        if (parseMessage(val, &newState, toolName, sizeof(toolName))) {
            g_callback(newState, toolName);
        }
    }
};

void bleInit(BLEStateCallback callback) {
    g_callback = callback;

    NimBLEDevice::init("ClaudeWatcher");

    NimBLEServer* pServer = NimBLEDevice::createServer();

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    NimBLECharacteristic* pChar = pService->createCharacteristic(
        CHAR_UUID,
        NIMBLE_PROPERTY::WRITE_NR   // write without response
    );
    pChar->setCallbacks(new CharCallbacks());

    pService->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->start();

    Serial.println("[BLE] Advertising as ClaudeWatcher");
}
