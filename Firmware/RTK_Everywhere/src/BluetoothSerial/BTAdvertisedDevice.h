/*
 * BTAdvertisedDevice.h
 *
 *  Created on: Feb 5, 2021
 *      Author: Thomas M. (ArcticSnowSky)
 */

#ifndef __BTADVERTISEDDEVICE_H__
#define __BTADVERTISEDDEVICE_H__

#include "BTAddress.h"


class BTAdvertisedDevice {
public:
    virtual ~BTAdvertisedDevice() = default;

    virtual BTAddress   getAddress();
    virtual uint32_t    getCOD() const;
    virtual String getName() const;
    virtual int8_t      getRSSI() const;


    virtual bool        haveCOD() const;
    virtual bool        haveName() const;
    virtual bool        haveRSSI() const;

    virtual String toString();
};

class BTAdvertisedDeviceSet : public virtual BTAdvertisedDevice {
public:
    BTAdvertisedDeviceSet();
    //~BTAdvertisedDeviceSet() = default;
    

    BTAddress   getAddress();
    uint32_t    getCOD() const;
    String getName() const;
    int8_t      getRSSI() const;


    bool        haveCOD() const;
    bool        haveName() const;
    bool        haveRSSI() const;

    String toString();

    void setAddress(BTAddress address);
    void setCOD(uint32_t cod);
    void setName(String name);
    void setRSSI(int8_t rssi);

    bool m_haveCOD;
    bool m_haveName;
    bool m_haveRSSI;


    BTAddress   m_address = BTAddress((uint8_t*)"\0\0\0\0\0\0");
    uint32_t    m_cod;
    String m_name;
    int8_t      m_rssi;
};

#endif
