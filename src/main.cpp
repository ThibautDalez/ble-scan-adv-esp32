#include <Arduino.h>
#include "sys/time.h"

#include "BLEDevice.h"

#include "BLEUtils.h"

#include "BLEServer.h"

#include "BLEBeacon.h"

#include "esp_sleep.h"
#include <BLEAdvertisedDevice.h>
#include <BLEScan.h>

#ifndef BEACON

class IBeaconAdvertised : public BLEAdvertisedDeviceCallbacks
{
public:
  // Callback when BLE is detected
  void onResult(BLEAdvertisedDevice device)
  {
    if (!isIBeacon(device))
    {
      return;
    }
    printIBeacon(device);
  }

private:
  // iBeacon packet judgment
  bool isIBeacon(BLEAdvertisedDevice device)
  {
    if (device.getManufacturerData().length() < 25)
    {
      return false;
    }
    if (getCompanyId(device) != 0x004C)
    {
      return false;
    }
    if (getIBeaconHeader(device) != 0x1502)
    {
      return false;
    }
    return true;
  }

  // CompanyId acquisition
  unsigned short getCompanyId(BLEAdvertisedDevice device)
  {
    const unsigned short *pCompanyId = (const unsigned short *)&device
                                           .getManufacturerData()
                                           .c_str()[0];
    return *pCompanyId;
  }

  // Get iBeacon Header
  unsigned short getIBeaconHeader(BLEAdvertisedDevice device)
  {
    const unsigned short *pHeader = (const unsigned short *)&device
                                        .getManufacturerData()
                                        .c_str()[2];
    return *pHeader;
  }

  // Get UUID
  String getUuid(BLEAdvertisedDevice device)
  {
    const char *pUuid = &device.getManufacturerData().c_str()[4];
    char uuid[64] = {0};
    sprintf(
        uuid,
        "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        pUuid[0], pUuid[1], pUuid[2], pUuid[3], pUuid[4], pUuid[5], pUuid[6], pUuid[7],
        pUuid[8], pUuid[9], pUuid[10], pUuid[11], pUuid[12], pUuid[13], pUuid[14], pUuid[15]);
    return String(uuid);
  }

  // Get TxPower
  signed char getTxPower(BLEAdvertisedDevice device)
  {
    const signed char *pTxPower = (const signed char *)&device
                                      .getManufacturerData()
                                      .c_str()[24];
    return *pTxPower;
  }

  // Serial output of iBeacon information
  void printIBeacon(BLEAdvertisedDevice device)
  {
    if (! (getUuid(device).equals("2686F39C-BADA-4658-854A-A62E7E5E8B8") || getUuid(device).equals("2686F39C-BADA-4658-854A-A62E7E5E8B8D"))){
      Serial.printf("addr:%s rssi:%d uuid:%s power:%d\r\n",
                    device.getAddress().toString().c_str(),
                    device.getRSSI(),
                    getUuid(device).c_str(),
                    *(signed char *)&device.getManufacturerData().c_str()[24]);
    }
  }
};

void setup()
{
  Serial.begin(115200);
  BLEDevice::init("");
}

void loop()
{
  BLEScan *scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new IBeaconAdvertised(), true);
  scan->setActiveScan(true);
  scan->start(1, false);
}
#else

#define GPIO_DEEP_SLEEP_DURATION 10 // sleep x seconds and then wake up

RTC_DATA_ATTR static time_t last; // remember last boot in RTC Memory

RTC_DATA_ATTR static uint32_t bootcount; // remember number of boots in RTC Memory

// See the following for generating UUIDs:

// https://www.uuidgenerator.net/

BLEAdvertising *pAdvertising; // BLE Advertisement type

struct timeval now;

#define BEACON_UUID "87b99b2c-90fd-11e9-bc42-526af7764f64" // UUID 1 128-Bit (may use linux tool uuidgen or random numbers via https://www.uuidgenerator.net/)

void setBeacon()
{

  BLEBeacon oBeacon = BLEBeacon();

  oBeacon.setManufacturerId(0x4C00); // fake Apple 0x004C LSB (ENDIAN_CHANGE_U16!)

  oBeacon.setProximityUUID(BLEUUID(BEACON_UUID));

  oBeacon.setMajor((bootcount & 0xFFFF0000) >> 16);

  oBeacon.setMinor(bootcount & 0xFFFF);

  oBeacon.setSignalPower(-50);

  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();

  BLEAdvertisementData oScanResponseData = BLEAdvertisementData();

  oAdvertisementData.setFlags(0x04); // BR_EDR_NOT_SUPPORTED 0x04

  std::string strServiceData = "";

  strServiceData += (char)26; // Len

  strServiceData += (char)0xFF; // Type

  strServiceData += oBeacon.getData();

  oAdvertisementData.addData(strServiceData);

  pAdvertising->setAdvertisementData(oAdvertisementData);

  pAdvertising->setScanResponseData(oScanResponseData);
}

void setup()
{

  Serial.begin(115200);

  gettimeofday(&now, NULL);

  Serial.printf("start ESP32 %d\n", bootcount++);

  Serial.printf("deep sleep (%lds since last reset, %lds since last boot)\n", now.tv_sec, now.tv_sec - last);

  last = now.tv_sec;

  // Create the BLE Device

  BLEDevice::init("ESP32 as iBeacon");

  BLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_ADV);

      // Create the BLE Server

  BLEServer *pServer = BLEDevice::createServer(); // <-- no longer required to instantiate BLEServer, less flash and ram usage

  pAdvertising = BLEDevice::getAdvertising();

  BLEDevice::startAdvertising();

  setBeacon();

  // Start advertising

  pAdvertising->start();

  Serial.println("Advertizing started...");

  delay(100);

  pAdvertising->stop();

  Serial.printf("enter deep sleep\n");

  esp_deep_sleep(1000000LL * GPIO_DEEP_SLEEP_DURATION);

  Serial.printf("in deep sleep\n");
}

void loop()
{
}
#endif