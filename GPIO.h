#pragma once
#include <vector>
#include <thread>

#include "equnit.h"

class GPIO : public equnit {
public:
	static std::shared_ptr<GPIO> create(const std::string &deviceTypeId, const std::string &deviceTypeName, const std::string &deviceTypeCode,
		const std::string &deviceName, const std::string &deviceCode) {
		auto gpioObj = std::make_shared<GPIO>(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName);
		if (!gpioObj->init_() || !gpioObj->startThread_()) {
			return nullptr;
		}

		return gpioObj;
	}

    void addTriggerPin(int pin);

private:
	void *gpioHandle_ = nullptr;

	struct triggerPin {
		int pin;
		unsigned char lastLevel;
	};
	std::vector<triggerPin> triggerPins;

	std::thread threadHandle_;
	bool isThreadRunning_ = false;
	static void mainWorkThread(void *param);

	bool init_();
	void destroy_();
	bool startThread_();
	void stopThread_();

	int getPinLevel_(const int pinNumber, unsigned char &level);
	int setPinLevel_(int pinNumber, unsigned char level);

	GPIO(std::string deviceCode): equnit("GPIO", deviceCode) {}
	GPIO(std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, 
		std::string deviceName, std::string deviceCode) :
		equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceName, deviceCode) {}
	~GPIO() { destroy_(); stopThread_(); }
};
