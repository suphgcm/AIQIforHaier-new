#pragma once
#include <vector>
#include "nlohmann/json.hpp"

#include "equnit.h"
#include "MvCodeReaderCtrl/MvCodeReaderCtrl.h"
#include "MvCodeReaderCtrl/MvCodeReaderParams.h"

class CodeReader : public equnit
{
public:
    static std::shared_ptr<CodeReader> create(const std::string ipAddr, const nlohmann::json &deviceParamConfigList,
        const std::string &deviceTypeId, const std::string &deviceTypeName, const std::string &deviceTypeCode,
        const std::string &deviceName, const std::string &deviceCode) {
        auto CodeReaderObj = std::make_shared<CodeReader>(ipAddr, deviceTypeId, deviceTypeName, deviceTypeCode, deviceName, deviceCode);
        if (CodeReaderObj->init_()) {
            CodeReaderObj->setParamByJson(deviceParamConfigList);
            CodeReaderObj->startGrabbing_();

            return CodeReaderObj;
        }

        return nullptr;
    }

    bool setParamByJson(const nlohmann::json &deviceParamConfigList);
    void getCode(std::vector<std::string> &codeVec);

private:
    bool init_();
    void destroy_();
    bool startGrabbing_();
    bool stopGrabbing_();
    bool getCodeReaderByIpAddress_();

    CodeReader(std::string deviceTypeCode, std::string deviceCode):equnit(deviceTypeCode, deviceCode) {}
    CodeReader(std::string ipAddr, std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceName, std::string deviceCode) :
        equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceName, deviceCode), ipAddr_(ipAddr) {}
    ~CodeReader() { stopGrabbing_(); destroy_(); }

    std::string ipAddr_;
    void *handle_ = nullptr;
    MV_CODEREADER_DEVICE_INFO *devInfo_ = nullptr;

    int triggerLatency_;
};
