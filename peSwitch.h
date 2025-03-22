#pragma once
#include "equnit.h"
#include "GPIO.h"

class LightSwitch : public equnit
{
public:
    LightSwitch(int GPIOpin, bool StopFlag, UINT BaseMsg, std::string l_codereaderID, BOOLEAN istrigger, GPIO* gpio,
        std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceName, std::string deviceCode) :
        equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceName, deviceCode),
        pin(GPIOpin), stopFlag(StopFlag), peBaseMsg(BaseMsg), codereaderID(l_codereaderID), b_istrigger(istrigger), gpUnit(gpio) {
    }
    ~LightSwitch() {}
    BOOLEAN istrigger() { return b_istrigger; }
    BOOLEAN registcodreader(std::unordered_map<std::string, equnit*> msgmap) {
    }
    BOOLEAN isStopFlag() { return stopFlag; }
    int getBindPin() { return pin; }

private:
    int pin;
    std::string codereaderID;
    bool stopFlag = false;

    UINT peBaseMsg;

    BOOLEAN b_istrigger;
    GPIO* gpUnit;

};

