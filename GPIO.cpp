#include <vector>
#include <thread>

#include <Uhi.h>
#include "MessageQueue.h"
#include "GPIO.h"

extern class MessageQueue<struct gpioMsg> GpioMessageQueue;

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
	Uhi* hUhi = (Uhi*)gpioHandle_;

	if (hUhi->GetGpioState(pinNumber, state)) {
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

void GPIO::setPinLevel_(const char pinNumber, int level) {
	GPIO_STATE state;
	Uhi* hUhi = (Uhi*)gpioHandle_;

	if (level == 0) {
		state = GPIO_LOW;
	}
	else {
		state = GPIO_HIGH;
	}

	bool ret = hUhi->SetGpioState(pinNumber, state);
	if (ret != true) {
		return;
	}

	return;
}

int GPIO::ReadMultipleTimes(const char pinNumber, unsigned char& curState, const int times) {
	GPIO_STATE state;
	Uhi* hUhi = (Uhi*)gpioHandle_;

	bool retu = hUhi->GetWinIoInitializeStates();
	bool ret = hUhi->GetGpioState(pinNumber, state);
	if (ret != true) {
		std::cerr << "getPinLevel failed: " << "-1" << std::endl;
		return -1;
	}

	for (int i = 0; i < times - 1; ++i) {
		GPIO_STATE tempState = GPIO_LOW;
		bool ret = hUhi->GetGpioState(pinNumber, tempState);
		if (ret != true) {
			std::cerr << "getPinLevel failed: " << "-1" << std::endl;
			return -1;
		}
		if (tempState != state) {
			std::cerr << "Level transition!" << std::endl;
			return -1;
		}
	}

	if (state == GPIO_LOW)
	{
		curState = 0;
	}
	else
	{
		curState = 1;
	}
	return 0;
}

void GPIO::mainWorkThread(void *param) {




//	MessageBox(NULL, L"GPIO线程启动!", L"GPIO", MB_OK);
	long requiredDur = 600;

	auto now = std::chrono::system_clock::now();
	auto timeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	long long timeMillisCount = timeMillis.count();
	long long lastChange[8];
	for (int i = 0; i < 8; ++i) {
		lastChange[i] = timeMillisCount;
	}
	
	long pinLevel[8] = { 1, 1, 1, 1, 1, 0, 0, 0 };

	GPIO* gpio = static_cast<GPIO*>(lpParam);
	while (gpio->is_thread_running == true) {
		for (char i = 1; i < 5; i++) {
//       for (char i = 2; i < 3; i++) {
			// 检测 6, 4, 2
			unsigned char curState = 2;
			gpio->ReadMultipleTimes(i, curState, 10);

			if (curState == 0 && pinLevel[i] == 1) {
				// 高变低
				auto now = std::chrono::system_clock::now();
				auto timeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
				long long timeMillisCount = timeMillis.count();
				if (timeMillisCount - lastChange[i] < requiredDur) {
					continue;
				}
				log_info("Gpio state change from high to low!");
// 触发
				if (gpio->msgmap.find(i) != gpio->msgmap.end()) {
					//SendMessage(gpio->msgmap[i], gpio->GPIOBASEMSG + 1, i, i);
					struct gpioMsg msg;
					msg.gpioPin = i;
					msg.message = gpio->GPIOBASEMSG + 1;
					GpioMessageQueue.push(msg);
				}
				pinLevel[i] = 0;
				lastChange[i] = timeMillisCount;
			}
			if (curState == 1 && pinLevel[i] == 0) {
				// 低变高
				auto now = std::chrono::system_clock::now();
				auto timeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
				long long timeMillisCount = timeMillis.count();
				if (timeMillisCount - lastChange[i] < requiredDur) {
					continue;
				}
				log_info("Gpio state change from low to high!");
// 触发结束
				if (gpio->msgmap.find(i) != gpio->msgmap.end()) {
					//SendMessage(gpio->msgmap[i], gpio->GPIOBASEMSG - 1, i, i);
					struct gpioMsg msg;
					msg.gpioPin = i;
					msg.message = gpio->GPIOBASEMSG - 1;
					GpioMessageQueue.push(msg);
				}
				pinLevel[i] = 1;
				lastChange[i] = timeMillisCount;
			}
		}
		//Sleep(requiredDur);
	}

	return 0;
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
}
