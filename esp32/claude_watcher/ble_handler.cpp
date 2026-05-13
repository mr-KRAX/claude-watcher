// esp32/claude_watcher/ble_handler.cpp
#include "ble_handler.h"
#include <NimBLEDevice.h>

#define SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define CHAR_UUID    "12345678-1234-1234-1234-123456789abd"

static BLEStateCallback g_stateCallback = nullptr;
static BLENotifCallback g_notifCallback = nullptr;

class CharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        std::string raw = pChar->getValue();
        String val(raw.c_str());
        Serial.println("[BLE] " + val);

        if (isNotification(val)) {
            if (g_notifCallback) g_notifCallback();
            return;
        }

        State newState = State::IDLE;
        char toolName[32] = "";
        if (parseMessage(val, &newState, toolName, sizeof(toolName))) {
            if (g_stateCallback) g_stateCallback(newState, toolName);
        }
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.println("[BLE] Disconnected — restarting advertising");
        NimBLEDevice::getAdvertising()->start();
    }
};

void bleInit(BLEStateCallback stateCallback, BLENotifCallback notifCallback) {
    g_stateCallback = stateCallback;
    g_notifCallback = notifCallback;

    NimBLEDevice::init("ClaudeWatcher");

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    NimBLECharacteristic* pChar = pService->createCharacteristic(
        CHAR_UUID,
        NIMBLE_PROPERTY::WRITE_NR
    );
    pChar->setCallbacks(new CharCallbacks());

    pService->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->start();

    Serial.println("[BLE] Advertising as ClaudeWatcher");
}
