#include "ble_manager.h"

static BLEManager bleManager;
QueueHandle_t writeCommandQueue;

BLEManager::BLEManager()
{
}

struct CharacteristicInfo
{
    const char *uuid;
    BLECharacteristic *characteristic;
    std::function<void(BLECharacteristic *, const UpdateMessage &)> updateFunction;
};

std::map<UpdateType, CharacteristicInfo> characteristicMap = {
    {STATUS_UPDATE, {CHARACTERISTIC_STATUS_UUID, nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                     {
                         c->setValue(m.data.statusUpdate);
                     }}},
    {WPOS_UPDATE, {CHARACTERISTIC_WPOS_UUID, nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                   {
                       c->setValue(m.data.wposUpdate);
                   }}},
    {FS_UPDATE, {CHARACTERISTIC_FS_UUID, nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                 {
                     c->setValue(m.data.fsUpdate);
                 }}},
    {PIN_UPDATE, {CHARACTERISTIC_PIN_UUID, nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                  {
                      c->setValue(m.data.pinUpdate);
                  }}},
    {QUEUE_ITEMS_UPDATE, {CHARACTERISTIC_QUEUE_UUID, nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                          {
                              c->setValue(m.data.queueItemsUpdate);
                          }}},
    {WCO_UPDATE, {CHARACTERISTIC_WCO_UUID, nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
                  {
                      c->setValue(m.data.wcoUpdate);
                  }}},
    {OV_UPDATE, {CHARACTERISTIC_OV_UUID, nullptr, [](BLECharacteristic *c, const UpdateMessage &m)
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

    NimBLECharacteristic *writeCharacteristics = pService->createCharacteristic(CHARACTERISTIC_WRITE_UUID, NIMBLE_PROPERTY::WRITE_NR);
    writeCharacteristics->setCallbacks(&sendCommandCallback);

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);

    BLEDevice::startAdvertising();

    writeCommandQueue = xQueueCreate(10, sizeof(char *));

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