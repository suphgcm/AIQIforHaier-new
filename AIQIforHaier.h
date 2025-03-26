#pragma once

#include <thread>
#include <mutex>
#include <fstream>
#include "product2btest.h"
#include "equnit.h"
#include "Camera.h"
#include "CodeReader.h"
#include "AudioEquipment.h"
#include "SerialCommunication.h"
#include <filesystem>
#include <iostream>

std::string projDir = ".";
std::unordered_map<int, std::vector<std::string>> trigger_map;
std::unordered_map<std::string, std::shared_ptr<equnit>> device_map;    // deviceCode - �豸
std::unordered_map<std::string, std::shared_ptr<Product>> product_map;  // productSnCode - Product

