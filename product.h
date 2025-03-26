#pragma once
#include <unordered_map>
#include <queue>

#include "equnit.h"
#include <nlohmann/json.hpp>

struct ProcessUnit {
	std::string productName;
	std::string productSnCode;
	std::string productSnModel;

	std::string processesCode;
	std::string processesTemplateCode;
	std::string processesTemplateName;

	std::string deviceTypeCode;
	std::string deviceCode;
	equnit *device;
	nlohmann::json param;
};

class Product {
public:
	Product() {}
	Product(std::string s_productSnCode, std::string s_productName, std::string s_productSnModel)
		: productSnCode(s_productSnCode), productName(s_productName), productSnModel(s_productSnModel) {}
	~Product() {}

	void addProcessUnit(int gpioPin, std::shared_ptr<ProcessUnit> unit);

private:
	std::string productSnCode;
	std::string productName;
	std::string productSnModel;
	std::unordered_map<int, std::queue<std::shared_ptr<ProcessUnit>>>testListMap;
};