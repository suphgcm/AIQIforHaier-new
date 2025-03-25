#include <iostream>
#include <thread>

#include <Uhi.h>
#include "GPIO.h"
#include "MessageQueue.h"

bool GPIO::init_() {
	gpioHandle_ = new(Uhi);
	if (gpioHandle_ == nullptr || false == ((Uhi *)gpioHandle_)->GetWinIoInitializeStates()) {
		return false;
	}

	return true;
}

void GPIO::destroy_() {
	if (gpioHandle_ == nullptr) {
		return;
	}

	delete(gpioHandle_);
	gpioHandle_ = nullptr;
	return;
}

int GPIO::getPinLevel_(int pinNumber, unsigned char &level) {
	GPIO_STATE state;
	Uhi *hUhi = (Uhi *)gpioHandle_;

	bool ret = hUhi->GetGpioState(pinNumber, state);
	if (ret != true) {
		return -1;
	}

	if (state == GPIO_LOW) {
		level = 0;
	}
	else {
		level = 1;
	}

	return 0;
}

int GPIO::setPinLevel_(int pinNumber, unsigned char level) {
	GPIO_STATE state;
	Uhi *hUhi = (Uhi *)gpioHandle_;

	if (level == 0) {
		state = GPIO_LOW;
	}
	else {
		state = GPIO_HIGH;
	}

	bool ret = hUhi->SetGpioState(pinNumber, state);
	if (ret != true) {
		return -1;
	}

	return 0;
}

void GPIO::mainWorkThread(void *param) {
	GPIO *gpio = static_cast<GPIO *>(param);

	while (gpio->isThreadRunning_ == true) {
		for (auto &triPin : gpio->triggerPins) {
			unsigned char level = 0;
			int ret = gpio->getPinLevel_(triPin.pin, level);
			if (-1 == ret) {
				continue;
			}

			unsigned char lastLevel = triPin.lastLevel;
			if (level == 1 && lastLevel == 0) {
				// TODO: trigger off
			}

			if (level == 0 && lastLevel == 1) {
				// TODO: trigger on
			}

			triPin.lastLevel = level;
		}
	}

	return;
}

bool GPIO::startThread_() {
	if (isThreadRunning_) {
		return true;
	}

	isThreadRunning_ = true;
	threadHandle_ = std::thread(mainWorkThread, this);

	return true;
}

void GPIO::stopThread_() {
	if (!isThreadRunning_) {
		return;
	}

	isThreadRunning_ = false;
	threadHandle_.join();

	return;
}

void GPIO::addTriggerPin(int pin) {
	struct triggerPin triPin = { 0 };

	triPin.pin = pin;
	triPin.lastLevel = 1;
	triggerPins.push_back(triPin);

	return;
}
