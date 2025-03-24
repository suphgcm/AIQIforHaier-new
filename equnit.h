#pragma once
#include <iostream>
#include <chrono>
#include <unordered_map>

static std::unordered_map<std::string, int> device_type_map = {
	{"GPIO", 0},
	{"Light", 1},
	{"Camera", 2},
	{"ScanningGun", 3},
	{"Speaker", 4},
	{"RemoteControl", 5},
    {"Microphone", 6}
};
static std::unordered_map<std::string, int> light_param_map = {
	{"linkedScanningGun", 0},//扫码枪编码
	{"devicePin", 1},//pin脚
	{"gpioDeviceCode", 2},//GPIO设备号
};
static std::unordered_map<std::string, int> camera_param_map = {
	{"acquisitionFrameRate", 0},//采集帧率
	{"exposureTime", 1},//曝光时间
	{"gain", 2},//增益
	{"triggerLatency", 3},//延迟开始工作时间（ms）
	{"acquisitionFrameCount", 4},//触发帧计数
	{"compressionQualit", 5},//图片压缩质量
	{"acquisitionFrameInterval", 6}
};
static std::unordered_map<std::string, int> scangun_param_map = {
	{"acquisitionFrameRate", 0},//采集帧率
	{"triggerLatency", 1},//延迟开始工作时间（ms）
	{"exposureTime", 2},//曝光时间 
	{"gain", 3},//增益
	{"acquisitionBurstFrameCount", 4},//触发帧计数
	{"lightSelectorEnable", 5},//是否打开所有光源
	{"currentPosition", 6}//电机步长（mm）
};
static std::unordered_map<std::string, int> microphone_param_map = {
	{"audioFile", 0}//唤醒语音文件路径
};
static std::unordered_map<std::string, int> serialport_param_map = {
	{"baudRate", 0},//波特率
	{"portName", 1},//com口
	{"message", 2},//需发送的消息
};

class equnit
{
public:
	std::string getDeviceTypeCode() {
		return deviceTypeCode_;
	}
	std::string getDeviceCode() {
		return deviceCode_;
	}

protected:
	equnit(std::string deviceTypeCode, std::string deviceCode) { deviceTypeCode_ = deviceTypeCode; deviceCode_ = deviceCode; }
	equnit(std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceName, std::string deviceCode) :
		deviceTypeId_(deviceTypeId), deviceTypeName_(deviceTypeName), deviceTypeCode_(deviceTypeCode), deviceName_(deviceName), deviceCode_(deviceCode) {}
	virtual  ~equnit() {}

private:
	std::string deviceTypeId_;
	std::string deviceTypeName_;
	std::string deviceTypeCode_;
	std::string deviceCode_;
	std::string deviceName_;
};