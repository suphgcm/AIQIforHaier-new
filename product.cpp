#include "product.h"

void Product::addProcessUnit(int gpioPin, std::shared_ptr<ProcessUnit> unit) {
	if (testListMap.find(gpioPin) == testListMap.end()) {
		std::queue<std::shared_ptr<ProcessUnit>> unitQueue;
		testListMap.insert(std::make_pair(gpioPin, unitQueue));
	}

	auto &unitQue = testListMap.find(gpioPin)->second;
	unitQue.push(unit);

	return;
}