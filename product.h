#pragma once
#include <mutex>
#include <iostream>
#include <windows.h>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <chrono>
#include "equnit.h"
#include "GPIO.h"
#include "peSwitch.h"
#include "Camera.h"
#include "CodeReader.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <unordered_map>

struct ProcessUnit {
	std::string productName;
	std::string productSn;//����
	std::string productSnCode;//ǰ9λ
	std::string productSnModel;

	std::string processesCode;
	std::string processesTemplateCode;
	std::string processesTemplateName;
	std::string audioFileName;

	ProcessUnit* prevunit = nullptr;
	ProcessUnit* nextunit = nullptr;

	int pin;

	long timestampms;//ִ��ʱ��
	std::string deviceTypeCode;
	std::string deviceCode;
	equnit* eq;
	long laterncy = LONG_MAX;//���trigger�������ӳ�ʱ�䣻
	nlohmann::json parameter;	
};

ProcessUnit* insertProcessUnit(ProcessUnit* listhead, ProcessUnit* node);

class Product {
public:
	std::string productSnCode;
	std::string productName;
	std::string productSnModel;
	std::unordered_map<std::string, equnit*>* deviceMap = NULL; //�豸�б� not used now
	std::unordered_map<int, std::vector<std::string>* >* processCodeMap = NULL;// GPIO pin - ProcessCode �б�
	std::unordered_map<int, ProcessUnit* >* testListMap = NULL; // GPIO pin - Process ���� 
public:
	Product() {}
	Product(std::string s_productSnCode, std::string s_productName, std::string s_productSnModel) 
		: productSnCode(s_productSnCode), productName(s_productName), productSnModel(s_productSnModel) {}
	~Product() {
		for (const auto& pair : *processCodeMap) {
			delete(pair.second);
		}
		delete(processCodeMap);
		processCodeMap = nullptr;

		for (const auto& pair : *testListMap) {
			auto head = pair.second;
			auto curUnit = head->nextunit;
			auto nextUnit = curUnit->nextunit;
			while (curUnit != head) {
				delete(curUnit);
				curUnit = nextUnit;
				nextUnit = curUnit->nextunit;
			}
			delete(head);
		}
		delete(testListMap);
		testListMap = nullptr;
	}

	void SetDeviceMap(std::unordered_map<std::string, equnit*>* deviceMap) {
		this->deviceMap = deviceMap;
	}

	void SetProcessCodeMap(std::unordered_map<int, std::vector<std::string>* >* processCodeMap) {
		this->processCodeMap = processCodeMap;
	}

	void SetTestListMap(std::unordered_map<int, ProcessUnit* >* testListMap) {
		this->testListMap = testListMap;
	}
};