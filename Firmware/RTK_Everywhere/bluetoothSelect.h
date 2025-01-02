#ifdef COMPILE_BT

// We use a local copy of the BluetoothSerial library so that we can increase the RX buffer. See issues:
// https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/23
// https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/469
#include "src/BluetoothSerial/BluetoothSerial.h"

#include <BleSerial.h> //Click here to get the library: http://librarymanager/All#ESP32_BleSerial by Avinab Malla

class BTSerialInterface
{
  public:
    virtual bool begin(String deviceName, bool isMaster, bool disableBLE, uint16_t rxQueueSize, uint16_t txQueueSize,
                       const char *serviceID, const char *rxID, const char *txID) = 0;

    virtual void disconnect() = 0;
    virtual void end() = 0;
    // virtual esp_err_t register_callback(esp_spp_cb_t callback) = 0;
    virtual void setTimeout(unsigned long timeout) = 0;

    virtual int available() = 0;
    virtual size_t readBytes(uint8_t *buffer, size_t bufferSize) = 0;
    virtual int read() = 0;

    // virtual bool isCongested() = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) = 0;
    virtual size_t write(uint8_t value) = 0;
    virtual void flush() = 0;
    virtual bool connected() = 0;
};

class BTClassicSerial : public virtual BTSerialInterface, public BluetoothSerial
{
    // Everything is already implemented in BluetoothSerial since the code was
    // originally written using that class
  public:
    bool begin(String deviceName, bool isMaster, bool disableBLE, uint16_t rxQueueSize, uint16_t txQueueSize,
               const char *serviceID, const char *rxID, const char *txID)
    {
        return BluetoothSerial::begin(deviceName, isMaster, disableBLE, rxQueueSize, txQueueSize);
    }

    void disconnect()
    {
        BluetoothSerial::disconnect();
    }

    void end()
    {
        BluetoothSerial::end();
    }

    // esp_err_t register_callback(esp_spp_cb_t callback)
    // {
    //     return BluetoothSerial::register_callback(callback);
    // }

    void setTimeout(unsigned long timeout)
    {
        BluetoothSerial::setTimeout(timeout);
    }

    int available()
    {
        return BluetoothSerial::available();
    }

    size_t readBytes(uint8_t *buffer, size_t bufferSize)
    {
        return BluetoothSerial::readBytes(buffer, bufferSize);
    }

    int read()
    {
        return BluetoothSerial::read();
    }

    size_t write(const uint8_t *buffer, size_t size)
    {
        return BluetoothSerial::write(buffer, size);
    }

    size_t write(uint8_t value)
    {
        return BluetoothSerial::write(value);
    }

    void flush()
    {
        BluetoothSerial::flush();
    }

    bool connected()
    {
        return (BluetoothSerial::connected());
    }
};

class BTLESerial : public virtual BTSerialInterface, public BleSerial
{
  public:
    // Missing from BleSerial
    bool begin(String deviceName, bool isMaster, bool disableBLE, uint16_t rxQueueSize, uint16_t txQueueSize,
               const char *serviceID, const char *rxID, const char *txID)
    {
        BleSerial::begin(deviceName.c_str(), serviceID, rxID, txID, -1); // name, service_uuid, rx_uuid, tx_uuid, led_pin
        return true;
    }

    void disconnect()
    {
        // Server->disconnect(Server->getConnId());
        // No longer used in v2 of BleSerial
    }

    void end()
    {
        BleSerial::end();
    }

    // esp_err_t register_callback(esp_spp_cb_t callback)
    // {
    //     connectionCallback = callback;
    //     return ESP_OK;
    // }

    void setTimeout(unsigned long timeout)
    {
        BleSerial::setTimeout(timeout);
    }

    int available()
    {
        return BleSerial::available();
    }

    size_t readBytes(uint8_t *buffer, size_t bufferSize)
    {
        return BleSerial::readBytes(buffer, bufferSize);
    }

    int read()
    {
        return BleSerial::read();
    }

    size_t write(const uint8_t *buffer, size_t size)
    {
        return BleSerial::write(buffer, size);
    }

    size_t write(uint8_t value)
    {
        return BleSerial::write(value);
    }

    void flush()
    {
        BleSerial::flush();
    }

    bool connected()
    {
        return (BleSerial::connected());
    }

    // Callbacks removed in v2 of BleSerial. Using polled connected() in bluetoothUpdate()
    // override BLEServerCallbacks
    // void Server->onConnect(BLEServer *pServer)
    // {
    //     connectionCallback(ESP_SPP_SRV_OPEN_EVT, nullptr); // Trigger callback to bluetoothCallback()
    // }

    // void onDisconnect(BLEServer *pServer)
    // {
    //     connectionCallback(ESP_SPP_CLOSE_EVT, nullptr); // Trigger callback to bluetoothCallback()
    //     // Server->startAdvertising(); //No longer used in v2 of BleSerial
    // }

  private:
    esp_spp_cb_t connectionCallback;
};

#endif // COMPILE_BT
