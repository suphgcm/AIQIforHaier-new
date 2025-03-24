#pragma once
#include "equnit.h"
#include "MvCameraControl/MvCameraControl.h"
#include "MvCameraControl/CameraParams.h"
#include "nlohmann/json.hpp"

class Camera : public equnit
{
public:
    static std::shared_ptr<Camera> create(const std::string ipAddr, const nlohmann::json & deviceParamConfigList,
        const std::string & deviceTypeId, const std::string & deviceTypeName, const std::string & deviceTypeCode,
        const std::string & deviceName, const std::string & deviceCode) {
        auto cameraObj = std::make_shared<Camera>(ipAddr, deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName);
        if (cameraObj->init_()) {
            cameraObj->setParamByJson(deviceParamConfigList);
            cameraObj->startGrabbing_();

            return cameraObj;
        }

        return nullptr;
    }

    bool setParamByJson(const nlohmann::json &deviceParamConfigList);
    bool getImage();

private:
    bool init_();
    void destroy_();
    bool startGrabbing_();
    bool stopGrabbing_();
    bool getCameraByIpAddress_();

    Camera(std::string ipAddr, std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode,
        std::string deviceName, std::string deviceCode) :
        equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceName, deviceCode), ipAddr_(ipAddr) {}
    ~Camera() { stopGrabbing_();  destroy_(); }

    std::string ipAddr_;
    MV_CC_DEVICE_INFO *devInfo_;
    void *handle_ = nullptr;

    int acquisitionFrameInterval_;
    int acquisitionFrameCount_;
    int triggerLatency_;
    int compressionQuality_;
};
