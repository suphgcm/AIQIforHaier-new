#pragma once
#include <iostream>
#include <chrono>
#include <unordered_map>

static std::unordered_map<std::string, int> device_type_map = {
	{"GPIO",0},
	{"light",1},
	{"Camera",2},
	{"ScanningGun",3},
	{"Speaker",4},
	{"RemoteControl",5},
    {"Microphone",6}
};
static std::unordered_map<std::string, int> light_param_map = {
	{"linkedScanningGun", 0},//ɨ��ǹ����
	{"devicePin", 1},//pin��
	{"GPIODeviceCode", 2},//GPIO�豸��
};
static std::unordered_map<std::string, int> camera_param_map = {
	{"acquisitionFrameRate", 0},//�ɼ�֡��
	{"exposureTime", 1},//�ع�ʱ��
	{"gain", 2},//����
	{"devicelatency", 3},//�ӳٿ�ʼ����ʱ�䣨ms��
	{"acquisitionBurstFrameCount", 4},//����֡����
	{"compressionQualit", 5},//ͼƬѹ������
	{"camera_interval", 6}
};
static std::unordered_map<std::string, int> scangun_param_map = {
	{"acquisitionFrameRate", 0},//�ɼ�֡��
	{"devicelatency", 1},//�ӳٿ�ʼ����ʱ�䣨ms��
	{"exposureTime", 2},//�ع�ʱ�� 
	{"gain", 3},//����
	{"acquisitionBurstFrameCount", 4},//����֡����
	{"lightSelectorEnable", 5},//�Ƿ�����й�Դ
	{"currentPosition", 6}//���������mm��
};
static std::unordered_map<std::string, int> microphone_param_map = {
	{"audiofile1", 0}//���������ļ�·��
};
static std::unordered_map<std::string, int> serialport_param_map = {
	{"baudRate",0},//������
	{"portName",1},//com��
	{"Message",2},//�跢�͵���Ϣ
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
		deviceTypeId_(deviceTypeId), deviceTypeName_(deviceTypeName), deviceTypeCode_(deviceTypeCode), deviceCode_(deviceCode), deviceName_(deviceName) {}
	virtual  ~equnit() {}

private:
	std::string deviceTypeId_;
	std::string deviceTypeName_;
	std::string deviceTypeCode_;
	std::string deviceCode_;
	std::string deviceName_;
};