#include "ble_manager.h"

static BLEManager bleManager;
QueueHandle_t writeCommandQueue;

BLEManager::BLEManager()
{
}

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

struct CharacteristicInfo
{
    const char *uuid;
    BLECharacteristic *characteristic;
    std::function<void(BLECharacteristic *, const UpdateMessage &)> updateFunction;
};

std::map<UpdateType, CharacteristicInfo> characteristicMap = {
    {STATUS_UPDATE, {"00000001-0000-1000-8000-00805f9b34fb", nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                     {
                         c->setValue(m.data.statusUpdate);
                     }}},
    {WPOS_UPDATE, {"00000002-0000-1000-8000-00805f9b34fb", nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                   {
                       c->setValue(m.data.wposUpdate);
                   }}},
    {FS_UPDATE, {"00000003-0000-1000-8000-00805f9b34fb", nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                 {
                     c->setValue(m.data.fsUpdate);
                 }}},
    {PIN_UPDATE, {"00000004-0000-1000-8000-00805f9b34fb", nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                  {
                      c->setValue(m.data.pinUpdate);
                  }}},
    {QUEUE_ITEMS_UPDATE, {"00000005-0000-1000-8000-00805f9b34fb", nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                          {
                              c->setValue(m.data.queueItemsUpdate);
                          }}},
    {WCO_UPDATE, {"00000006-0000-1000-8000-00805f9b34fb", nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                  {
                      c->setValue(m.data.wcoUpdate);
                  }}},
    {OV_UPDATE, {"00000007-0000-1000-8000-00805f9b34fb", nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                 {
                     c->setValue(m.data.ovUpdate);
                 }}}};

class SendCommandCharacteristicsCallback : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
    {
        if (strlen(pCharacteristic->getValue().c_str()) < 256)
        {
            char *command = strdup(pCharacteristic->getValue().c_str());
            if (!xQueueSend(writeCommandQueue, &command, portMAX_DELAY))
            {
                free(command);
            }
        }

        pCharacteristic->setValue("");
    }
};

static SendCommandCharacteristicsCallback sendCommandCallback;

BLECharacteristic *createCharacteristic(BLEService *pService, const char *uuid)
{
    return pService->createCharacteristic(
        uuid,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
}

void ble_manager_task(void *pvParameters)
{
    UpdateMessage message;
    while (1)
    {
        if (xQueueReceive(xQueueUpdates, &message, portMAX_DELAY))
        {
            auto it = characteristicMap.find(message.type);
            if (it != characteristicMap.end())
            {
                it->second.updateFunction(it->second.characteristic, message);
                it->second.characteristic->notify();
            }
        }
    }
}

void BLEManager::init()
{
    BLEDevice::init("OpenPendant");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    for (auto &pair : characteristicMap)
    {
        pair.second.characteristic = createCharacteristic(pService, pair.second.uuid);
    }

    NimBLECharacteristic *writeCharacteristics = pService->createCharacteristic("00000000-0000-1000-8000-00805f9b34fb", NIMBLE_PROPERTY::WRITE_NR);
    writeCharacteristics->setCallbacks(&sendCommandCallback);

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);

    BLEDevice::startAdvertising();

    writeCommandQueue = xQueueCreate(10, sizeof(char*));

    xTaskCreate(
        ble_manager_task,
        "ble_manager_task",
        4096,
        NULL,
        1,
        NULL);
}

extern "C" void ble_manager_init()
{
    bleManager.init();
}