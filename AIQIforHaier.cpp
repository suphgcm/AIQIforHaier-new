// AIQIforHaier.cpp : 定义应用程序的入口点。

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include <cstdio>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <codecvt>
#include <string>
#include <locale>
#include <unordered_map>
#include <filesystem>
#include <cstdlib>
#include <list>
#include <sstream>
#include <thread>
#include <chrono>
#include <limits>
#include <httplib.h>

#include "AIQIforHaier.h"
#include "MessageQueue.h"
#include "WZSerialPort.h"
#include "Log.h"
#include "resource.h"
#include "product.h"
#include "GPIO.h"

std::mutex mtx[8];
bool isPinTriggered[8]; // GPIO 针脚是否触发状态
int remoteCtrlPin;

std::string pipeline_code = "CX202309141454000002"; // 一台工控机只跑一个 pipeline
std::string server_ip = "192.168.0.189";
int server_port = 9003;


int stopFlagPin = 1;
std::string pipelineName;

// 线程函数
DWORD __stdcall MainWorkThread(LPVOID lpParam);
DWORD __stdcall UnitWorkThread(LPVOID lpParam);

struct counter {
	std::mutex mutex;
	long long count;
} Counter;

std::mutex map_mutex;

std::vector<char> readPCMFile(const std::string& filename) {
    // 打开文件
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    // 获取文件大小
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 创建一个向量来保存文件内容
    std::vector<char> buffer(size);

    // 读取文件
    if (file.read(buffer.data(), size)) {
        return buffer;
    } else {
        // 处理错误
        throw std::runtime_error("Error reading file");
    }
}

void AddTextPart(std::vector<char> &body, std::string &text, std::string &boundary, std::string name)
{
	std::string textPartStart = "Content-Disposition: form-data; name=\""+ name + \
		"\"\r\nContent-Type:text/plain\r\n\r\n";
	body.insert(body.end(), textPartStart.begin(), textPartStart.end());

	std::string textData = text;
	body.insert(body.end(), textData.begin(), textData.end());

	std::string textPartEnd = "\r\n--" + boundary + "\r\n";
	body.insert(body.end(), textPartEnd.begin(), textPartEnd.end());

	return;
}

void AddBinaryPart(std::vector<char>& body, unsigned char* buffer, unsigned int lengh, std::string& boundary, std::string fileName, MSG_TYPE_E binaryType)
{
	std::string contentType;
	if (binaryType == MSG_TYPE_PICTURE) {
		contentType = "image/jpeg";
	}
	else {
		contentType = "audio/basic";
	}

	// Add picture
	std::string partStart = "Content-Disposition: form-data; name=\"files\"; filename=\"" + \
		fileName + "\"\r\nContent-Type: " +  contentType + "\r\n\r\n";
	body.insert(body.end(), partStart.begin(), partStart.end());

	for (int i = 0; i < lengh; i++)
	{
		body.push_back(buffer[i]);
	}

	std::string partEnd = "\r\n--" + boundary + "\r\n";
	body.insert(body.end(), partEnd.begin(), partEnd.end());

	return;
}

void HttpPost(struct httpMsg& msg) {
	httplib::Client cli(serverIp, serverPort);

	std::string path;
	httplib::Headers headers;
	std::string body;
	std::string contentType;

	if (msg.type == MSG_TYPE_STOP) {
		path = "/api/client/inspection/stopFlag";
		//headers = { {"Content-Type", "application/json"} };
		contentType = "application/json";
		nlohmann::json jsonObject;
		jsonObject["pipelineCode"] = msg.pipelineCode;
		jsonObject["productSn"] = msg.productSn;
		body = jsonObject.dump();
	}
	else {
		path = "/api/client/inspection/upload";
		//headers = { {"Content-Type", "multipart/form-data;boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW"} };
		contentType = "multipart/form-data;boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW";
		std::string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
		std::vector<char> body1;

		//add first boundary
		std::string partStart = "--" + boundary + "\r\n";
		body1.insert(body1.end(), partStart.begin(), partStart.end());

		if (msg.type == MSG_TYPE_PICTURE) {
			for (auto it = msg.pictures.begin(); it != msg.pictures.end(); ++it)
			{
				std::string imageName = std::to_string(it->sampleTime) + ".jpeg";
				// Add picture
				AddBinaryPart(body1, it->imageBuffer, it->imageLen, boundary, imageName, MSG_TYPE_PICTURE);
			}
		}
		else if (msg.type == MSG_TYPE_TEXT) {
			// Add TEXT
			AddTextPart(body1, msg.text, boundary, "content");
		}
		else if (msg.type == MSG_TYPE_SOUND) {
			std::string soundPath = projDir.c_str();
			soundPath.append("\\temp\\");
			std::string soundName = std::to_string(msg.sampleTime) + ".pcm";
			if (!std::filesystem::exists(soundPath + soundName))
			{
				log_error("Target pcm file does not exist! fileName: " + soundName);
				return;
			}
			auto sound = readPCMFile(soundPath + soundName);
			// Add sound
			AddBinaryPart(body1, (unsigned char*)sound.data(), sound.size(), boundary, soundName, MSG_TYPE_PICTURE);
		}
		else {
			log_error("Invalid msg type, msgType: " + std::to_string(msg.type) + ", msgId: " + std::to_string(msg.msgId));
		}

		std::string sampleTime = "123456789";
		// Add text part
		AddTextPart(body1, msg.pipelineCode, boundary, "pipelineCode");
		AddTextPart(body1, msg.processesCode, boundary, "processesCode");
		AddTextPart(body1, msg.processesTemplateCode, boundary, "processesTemplateCode");
		AddTextPart(body1, msg.productSn, boundary, "productSn");
		AddTextPart(body1, msg.productSnCode, boundary, "productSnCode");
		AddTextPart(body1, msg.productSnModel, boundary, "productSnModel");
		AddTextPart(body1, sampleTime, boundary, "sampleTime");

		body1.pop_back();
		body1.pop_back();
		std::string End = "--\r\n";
		body1.insert(body1.end(), End.begin(), End.end());

		std::string temp(body1.begin(), body1.end());
		body = temp;
	}

	log_info("Start post http msg, msgId: " + std::to_string(msg.msgId));
	auto res = cli.Post(path.c_str(), headers, body, contentType);
	if (res) {
		if (res->status == 200) {
			log_info("Http msg post successed! msgId: " + std::to_string(msg.msgId));
		}
		else {
			log_error("Http msg post failed! resp code:" + std::to_string(res->status));
		}
	}
	else {
		auto err = res.error();
		log_error("Http msg post failed! http err:" + httplib::to_string(err));
	}
	log_info("End post http msg, msgId: " + std::to_string(msg.msgId));

	return;
}

DWORD HttpPostThread(LPVOID lpParam)
{
	struct httpMsg msg;

	while (true)
	{
		Singleton::instance().wait(msg);
		if (msg.type == MSG_TYPE_STOP)
		{
			log_info("Process http stop msg, msgId: " + std::to_string(msg.msgId) + \
				", processSn: " + msg.productSn);
		}
		else {
			log_info("Process http msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn + \
				", processesTemplateCode : " + msg.processesTemplateCode);
		}

		HttpPost(msg);

		if (msg.type == MSG_TYPE_STOP)
		{
			log_info("End process http stop msg, msgId: " + std::to_string(msg.msgId) + \
				", processSn: " + msg.productSn);
		}
		else {
			log_info("End process http msg, msgId: " + std::to_string(msg.msgId) + \
				", processSn: " + msg.productSn + ", processesTemplateCode : " + msg.processesTemplateCode);
		}

		if (msg.type == MSG_TYPE_PICTURE)
		{
			for (auto it = msg.pictures.begin(); it != msg.pictures.end(); ++it)
			{
				delete[] it->imageBuffer;
			}
		}
	}

	return 0;
}

class MessageQueue<struct gpioMsg> GpioMessageQueue;

void GpioMsgProc(struct gpioMsg& msg)
{
	int message = msg.message;
	UINT gpioPin = msg.gpioPin;

	switch (message)
	{
	case WM_GPIO_ON:
		log_info("Gpio " + std::to_string(gpioPin) + " triggered on!");
		TriggerOn(gpioPin);
		break;
	case WM_GPIO_OFF:
		TriggerOff(gpioPin);
		break;
	default:
		printf("Invalid gpio message.\n");
	}

	return;
}

// 各个按钮对应的函数
bool StartDeviceSelfTesting() {
	int testCount = 0;
	for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
		switch (deviceTypeCodemap[it->second->getDeviceTypeCode()]) {
		case 0: {//GPIO
			GPIO* deviceGPIO = dynamic_cast<GPIO*>(it->second);
			if (deviceGPIO->Init() && deviceGPIO->StartThread()) {
				log_info("GPIO "  + deviceGPIO->getDeviceCode() + " self test successed!");
				testCount++;
			}
			else {
				log_error("GPIO " + deviceGPIO->getDeviceCode() + " self test failed!");
			}
			break;
		}
		case 1: //光电开关自检默认通过
			log_info("Light switch self test successed!");
			testCount++;
			break;
		case 2: {//摄像机开关
			Camera* deviceCamera = dynamic_cast<Camera*>(it->second);
			if (deviceCamera->GetCameraByIpAddress() && deviceCamera->Init()) {
				deviceCamera->StartGrabbing();
				deviceCamera->GetImageX();
				log_info("Camera " + deviceCamera->getDeviceCode() + " self test successed!");
				testCount++;
			}
			else {
				log_error("Camera " + deviceCamera->getDeviceCode() + " self test failed!");
			}
			break;
		}
		case 3: {//码枪
			CodeReader* deviceCodeReader = dynamic_cast<CodeReader*>(it->second);
			if (deviceCodeReader->GetCodeReaderByIpAddress() && deviceCodeReader->Init()) {
				deviceCodeReader->StartGrabbing();
				log_info("Code Reader " + deviceCodeReader->getDeviceCode() + " self test successed!");
				testCount++;
			}
			else {
				log_error("Code Reader " + deviceCodeReader->getDeviceCode() + " self test failed!");
			}
			break;
		}
		case 4: { //扬声器speaker
			AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(it->second);
				
			std::string soundFilePath = projDir.c_str();
			soundFilePath.append("\\sounds\\");
			audioDevice->ReadFile(soundFilePath);

			//int ret = audioDevice->Init(soundFile); // todo: 解决未找到音频设备时抛出异常的问题
			log_info("Speaker " + audioDevice->getDeviceCode() + " self test successed!");
			testCount++; // todo: 加条件
			break;
		}
		case 5: { // 红外发射器
			SerialCommunication* scDevice = dynamic_cast<SerialCommunication*>(it->second);
			//if (scDevice->OpenSerial()) {
			log_info("IR transmitter " + scDevice->getDeviceCode() + " self test successed!");
			testCount++;
			//}
			//else {
			//	std::string logStr = "Remote control " + it->first + " selftest error!\n";
			//	AppendLog(StringToLPCWSTR(logStr));
			//}
			break;
		}
		case 6: {
			AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(it->second);
			log_info("Speaker " + audioDevice->getDeviceCode() + " self test successed!");
			testCount++;
			break;
		}
		default:
			break;
		}
	}

	if (testCount != deviceMap.size()) {
		return false;
	}

	return true;
}




DWORD __stdcall InfraredRemoteCtlThread(LPVOID lpParam) {
	CloseHandle(GetCurrentThread());
	log_info("Infrared remote controler be called!");
	WZSerialPort w;

	char cmdI[] = { 0xFF,0x16,0x06,0x16,0x00,0x32,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x80,0x00,0x00,0x00,0x0B, \
	0x7B,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x00,0x00,0x00,0x00,0x0B, \
	0xFB,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x80,0x00,0x00,0x00,0x0B, \
	0x7B,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x00,0x00,0x00,0x00,0x0B, \
	0xFB,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x80,0x00,0x00,0x00,0x0B, \
	0x7B,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x00,0x00,0x00,0x00,0x0B, \
	0xFB,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xAD,0x84 };

	char cmdG[] = { 0xFF, 0x16, 0x06, 0x14, 0x00, 0x32, 0xA6, 0xEC, 0x00, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x80, 0x00, \
		0x00, 0x00, 0x0B, 0x7D, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, 0xA6, 0xEC, 0x00, 0x00, 0xC0, 0x20, 0x00, 0x80, \
		0x00, 0x00, 0x00, 0x00, 0x0B, 0xFD, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, 0xA6, 0xEC, 0x00, 0x00, 0xC0, 0x20, \
		0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x0B, 0x7D, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, 0xA6, 0xEC, 0x00, 0x00, \
		0xC0, 0x20, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0B, 0xFD, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, 0xA6, 0xEC, \
		0x00, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x0B, 0x7D, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, \
		0xA6, 0xEC, 0x00, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0B, 0xFD, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, 0x6C, 0x63
	};

	char cmdH[] = { 0xFF, 0x16, 0x06, 0x14, 0x00, 0x0A, 0xA6, 0xEC, 0xE0, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x80, 0x00, \
		0x00, 0x00, 0x8B, 0xDD, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, 0xA6, 0xEC, 0xE0, 0x00, 0xC0, 0x20, 0x00, 0x80, \
		0x00, 0x00, 0x00, 0x00, 0x8B, 0x5D, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, 0xA6, 0xEC, 0xE0, 0x00, 0xC0, 0x20, \
		0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x8B, 0xDD, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, 0xA6, 0xEC, 0xE0, 0x00, \
		0xC0, 0x20, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x5D, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, 0xA6, 0xEC, \
		0xE0, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x8B, 0xDD, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, \
		0xA6, 0xEC, 0xE0, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x5D, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, 0x03, 0xDC
	};

	unsigned char rcvBuf[2048] = { 0 };
	if (w.open("com1"))
	{
		cout << "open success!" << std::endl;
		w.sendBytes(cmdI, sizeof(cmdI));
/*		Sleep(5000);
		int length = w.receive(rcvBuf, sizeof(rcvBuf));
		for (int i = 0; i < length; i++)
		{
			printf("0x%x ", rcvBuf[i]);
		}
		printf("\n");
*/
		w.close();
	}
	return 0;
}

struct UnitWorkPara
{
	bool sameProductSnCode;
	ProcessUnit* procUnit;
};

bool isStopFlagLight(int gpioPin) {
	bool StopFlag = false;
	for (const auto& pair : deviceMap) {
		equnit* device = pair.second;
		if (device->getDeviceTypeCode() != "light") {
			continue;
		}

		peSwitch* pSwitch = dynamic_cast<peSwitch*>(device);
		if (gpioPin == pSwitch->getBindPin()) {
			StopFlag = pSwitch->isStopFlag();
			break;
		}
	}

	return StopFlag;
}

DWORD __stdcall MainWorkThread(LPVOID lpParam) {
	CloseHandle(GetCurrentThread());
	int gpioPin = (int)lpParam;

	log_info("Gpiopin " + std::to_string(gpioPin) + ": Start scan product sn code!");
	// 得到产品序列号前9位
	std::vector<std::string> vcodereaders = triggerMaps[gpioPin];
	std::vector<std::string> codereaderresults;
	for (std::string codereader : vcodereaders) {
		auto it = deviceMap.find(codereader);
		if (it == deviceMap.end()) {
			log_warn("Gpiopin " + std::to_string(gpioPin) + ":Target codereader " + codereader + " doesn't exist, please check configure!");
			return 0;
		}
		CodeReader* CR = dynamic_cast<CodeReader*>(it->second);
		log_info("Gpiopin " + std::to_string(gpioPin) + ": CodeReader " + CR->e_deviceCode + " called!");
		std::vector<std::string> results;
		CR->Lock();
		int crRet = CR->ReadCode(results);
		CR->UnLock();
		codereaderresults.insert(codereaderresults.end(), results.begin(), results.end());
		if (!results.empty())
		{
			break;
		}
	}

	if (codereaderresults.empty()) {
		log_warn("Gpiopin " + std::to_string(gpioPin) + ": Scan product sn code result is null!");
		return 0;
	}
	std::string productSn = "";
	for (size_t i = 0; i < codereaderresults.size(); i++) {
		std::string tmpr = codereaderresults[i];
		// 取最长
		if (tmpr.size() > productSn.size()) {
			productSn = tmpr;
		}
	}
	if (productSn.length() < 13) {
		log_warn("Gpiopin " + std::to_string(gpioPin) + ": Scan  product sn code failed, product sn length less than 13!");
		return 0;
	}
	log_info("Gpiopin " + std::to_string(gpioPin) + ": End scan product sn code!");

	std::string productSnCode = productSn.substr(0, 9);

	std::unique_lock<std::mutex> lock(map_mutex);
	// 跳过 pipeline 配置里不存在的商品总成号 productSnCode
	auto productItem = productMap->find(productSnCode);
	if (productItem == productMap->end()) {
		log_warn("Gpiopin " + std::to_string(gpioPin) + ": Product sn " + productSnCode + " does not exist!");
		return 0;
	}
	auto product = productItem->second;
	lock.unlock();

	log_info("Gpiopin " + std::to_string(gpioPin) + ": Product sn " + productSn + " Scaned!");

	ProcessUnit* head =product->testListMap->find(gpioPin)->second;

	auto now0 = std::chrono::system_clock::now();
	auto duration_in_milliseconds0 = std::chrono::duration_cast<std::chrono::milliseconds>(now0.time_since_epoch());
	long startTime = duration_in_milliseconds0.count();

	auto it = map2bTest.find(gpioPin);
	if (it == map2bTest.end()) {
		log_warn("Gpiopin " + std::to_string(gpioPin) + ": Map2bTest init error!");
		return 0;
	}
    std::string lastProductSnCode = it->second->productSnCode;
	it->second->pinNumber = gpioPin;
	it->second->productSn = productSn;
	it->second->productSnCode = productSnCode;
	it->second->processUnitListHead = head;
	it->second->shotTimestamp = startTime;

	product2btest* myp2btest = it->second;
	std::vector<HANDLE> handles;
	auto now1 = std::chrono::system_clock::now();
	auto duration_in_milliseconds1 = std::chrono::duration_cast<std::chrono::milliseconds>(now1.time_since_epoch());
	long start = duration_in_milliseconds1.count();

	ProcessUnit* tmpFind = myp2btest->processUnitListHead->nextunit;
	int count = 0;
	int gpiopin = myp2btest->processUnitListHead->pin;
	while (tmpFind != myp2btest->processUnitListHead) {
		auto now2 = std::chrono::system_clock::now();
		auto duration_in_milliseconds2 = std::chrono::duration_cast<std::chrono::milliseconds>(now2.time_since_epoch());
		long runla = duration_in_milliseconds2.count();

		// 加句柄队列
		if ((runla - start) >= tmpFind->laterncy) {
			tmpFind->productSn = myp2btest->productSn;
			struct UnitWorkPara* param = new(struct UnitWorkPara);
			if (lastProductSnCode == productSnCode)
			{
				param->sameProductSnCode = TRUE;
			}
			else
			{
				param->sameProductSnCode = FALSE;
			}
			param->procUnit = tmpFind;
			HANDLE hUnitWork = CreateThread(NULL, 0, UnitWorkThread, param, 0, NULL);
			handles.push_back(hUnitWork);
			tmpFind = tmpFind->nextunit;
		}
		else {
			Sleep(10);
		}
	}

	for (auto& handle : handles) {
		WaitForSingleObject(handle, INFINITE);
		CloseHandle(handle);
	}


	/*最后一个gpio引脚触发事件处理结束后，发送检测结束标志*/
	if (isStopFlagLight(gpioPin))
	{
		Sleep(1500);
		struct httpMsg msg;
		Counter.mutex.lock();
		Counter.count++;
		msg.msgId = Counter.count;
		Counter.mutex.unlock();
		msg.pipelineCode = pipelineCode;
		msg.productSn = productSn + pipelineCode;
		msg.type = MSG_TYPE_STOP;

		Singleton::instance().push(msg);
		log_info("push stop msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn);
	}
	return 0;
}

void TriggerOn(UINT gpioPin)
{
	// 1. triggerMap 把引脚和扫码枪对应
	// 2. 根据扫码枪扫到的码，确定产品
	// 3. 根据产品确定执行的 process
	if (!mtx[gpioPin].try_lock()) {
		return;
	}

	if (isPinTriggered[gpioPin]) {
		mtx[gpioPin].unlock();
		return;
	}
	isPinTriggered[gpioPin] = true;

	HANDLE hdw = CreateThread(NULL, 0, MainWorkThread, (LPVOID)gpioPin, 0, NULL);
	return;
}

void deleteFile(const std::filesystem::path& filename) {
	try {
		if (std::filesystem::remove(filename)) {
			std::cout << "文件删除成功！\n";
		}
		else {
			std::cout << "文件不存在！\n";
		}
	}
	catch (std::filesystem::filesystem_error& e) {
		std::cout << "发生错误：" << e.what() << '\n';
	}
}

DWORD __stdcall UnitWorkThread(LPVOID lpParam) {
	struct UnitWorkPara *param = static_cast<struct UnitWorkPara *>(lpParam);
	ProcessUnit* unit = param->procUnit;
	bool sameProductSn = param->sameProductSnCode;
	delete(param);
	std::string path = projDir.c_str();

	switch (deviceTypeCodemap[unit->deviceTypeCode]) {
	case 2: { // Camera
		Camera* devicecm = dynamic_cast<Camera*>(unit->eq);
		log_info("Camera " + devicecm->e_deviceCode + " be called!");
		devicecm->Lock();
		if (sameProductSn == FALSE)
		{
			devicecm->SetValuesByJson(unit->parameter);
		}
		devicecm->GetImage(path, unit);
		devicecm->UnLock();
		break;
	}
	case 3: { // ScanningGun
		CodeReader* deviceCR = dynamic_cast<CodeReader*>(unit->eq);
		log_info("Scancoder " + deviceCR->e_deviceCode + " be called!");

		deviceCR->Lock();
		if (sameProductSn == FALSE)
		{
			deviceCR->SetValuesByJson(unit->parameter);
		}
		std::vector<std::string> codeRes;
		int crRet = deviceCR->ReadCode(codeRes);
		deviceCR->UnLock();

		struct httpMsg msg;
		Counter.mutex.lock();
		Counter.count++;
		msg.msgId = Counter.count;
		Counter.mutex.unlock();
		msg.pipelineCode = pipelineCode;
		msg.processesCode = unit->processesCode;
		msg.processesTemplateCode = unit->processesTemplateCode;
		msg.productSn = unit->productSn + pipelineCode;
		msg.productSnCode = unit->productSnCode;
		msg.productSnModel = unit->productSnModel;
		msg.type = MSG_TYPE_TEXT;
		if (!codeRes.empty()) {
			msg.text = "";
			for (size_t i = 0; i < codeRes.size(); ++i)
			{
				if (codeRes[i].length() > 0)
				{
					msg.text = codeRes[i];
					break;
				}
			}
		}
		else {
			msg.text = "";
		}
		Singleton::instance().push(msg);
		log_info("push msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn + ", processesTemplateCode : " + msg.processesTemplateCode);
		break;
	}
	case 4: { // Speaker
		AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(unit->eq);
		log_info("Speaker device " + audioDevice->e_deviceCode + " be called!");
/*		// 初始化
		int ret = audioDevice->Init(); // todo: 解决未找到音频设备时抛出异常的问题

		auto now = std::chrono::system_clock::now();
		auto duration = now.time_since_epoch();
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
	    std:string outFile = path + "\\temp\\" + std::to_string(milliseconds) + ".pcm";

		// 录制 todo: 考虑录制时间
		std::string recordFile = path + "\\temp"+ "\\audio_data.pcm";
		std::string logStr = "Record: ";
		logStr.append(recordFile).append("\n");
		AppendLog(StringToLPCWSTR(logStr));

		int audioRet = audioDevice->PlayAndRecord(recordFile, 7);
		logStr = "Ret: ";
		logStr.append(std::to_string(audioRet)).append("\n");
		AppendLog(StringToLPCWSTR(logStr));

		audioDevice->To16k(recordFile);
		std::string recordFile1 = path + "\\temp" + "\\audio_data_16.pcm";
		audioDevice->CutFile(recordFile1, outFile, 6, 7);

		// 删除文件
		deleteFile(recordFile);
		audioDevice->Terminate();
		if (0 == audioRet)
		{
			struct httpMsg msg;
			msg.pipelineCode = pipelineCode;
			msg.processesCode = unit->processesCode;
			msg.processesTemplateCode = unit->processesTemplateCode;
			msg.productSn = unit->productSn;
			msg.productSnCode = unit->productSnCode;
			msg.productSnModel = unit->productSnModel;
			msg.type = MSG_TYPE_SOUND;
			msg.sampleTime = milliseconds;
			Singleton::instance().push(msg);
		}
*/
		WAVEFORMATEX format;
		format.wFormatTag = WAVE_FORMAT_PCM;
		format.nChannels = 1;
		format.nSamplesPerSec = 48000;
		format.wBitsPerSample = 16;
		format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8;
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
		format.cbSize = 0;

		audioDevice->PlayAudio(&format, unit->audioFileName);
		break;
	}
	case 5: { // RemoteControl
		// todo: 加串口
		break;
	}
	case 6: { //Microphone
		AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(unit->eq);
		log_info("Microphone device " + audioDevice->e_deviceCode + " be called!");

		WAVEFORMATEX waveFormat;
		waveFormat.wFormatTag = WAVE_FORMAT_PCM;
		waveFormat.nSamplesPerSec = 48000;
		waveFormat.nChannels = 1;
		waveFormat.wBitsPerSample = 16;
		waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
		waveFormat.cbSize = 0;

		auto now = std::chrono::system_clock::now();
		auto duration = now.time_since_epoch();
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

		std::string recordFile = path + "\\temp" + "\\audio_data.pcm";
		int audioRet = audioDevice->RecordAudio(&waveFormat, 2, recordFile);
		std::string resultFile = path + "\\temp\\" + std::to_string(milliseconds) + ".pcm";
		audioDevice->To16k(recordFile, resultFile);

		// 删除文件
		deleteFile(recordFile);

		if (0 == audioRet)
		{
			struct httpMsg msg;
			Counter.mutex.lock();
			Counter.count++;
			msg.msgId = Counter.count;
			Counter.mutex.unlock();
			msg.pipelineCode = pipelineCode;
			msg.processesCode = unit->processesCode;
			msg.processesTemplateCode = unit->processesTemplateCode;
			msg.productSn = unit->productSn + pipelineCode;
			msg.productSnCode = unit->productSnCode;
			msg.productSnModel = unit->productSnModel;
			msg.type = MSG_TYPE_SOUND;
			msg.sampleTime = milliseconds;
			Singleton::instance().push(msg);
			log_info("push msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn + ", processesTemplateCode : " + msg.processesTemplateCode);
		}
		break;
	}
	default:
		break;
	}

	return 0;
}

void TriggerOff(UINT gpioPin)
{
	mtx[gpioPin].try_lock();
	mtx[gpioPin].unlock();
	isPinTriggered[gpioPin] = false;

	return;
}





void createDevice(const nlohmann::json &deviceConfig)
{
	std::string deviceTypeId, deviceTypeCode, deviceTypeName, deviceCode, deviceName;

	deviceCode = deviceConfig.at("deviceCode");
	deviceName = deviceConfig["deviceName"];
	deviceTypeCode = deviceConfig.at("deviceTypeCode");
	deviceTypeName = deviceConfig["deviceTypeName"];
	deviceTypeId = deviceConfig["deviceTypeId"];

	switch (device_type_map[deviceTypeCode]) {
	case 0: { //Gpio
		if (device_map.find(deviceCode) == device_map.end()) {
			auto gpioObj = GPIO::create(deviceTypeId, deviceTypeName, deviceTypeCode, deviceName, deviceCode);
			if (gpioObj) {
				device_map.insert(std::make_pair(deviceCode, gpioObj));
			}
			else {
				log_error("Gpio device " + deviceCode + " create failed!");
			}
		}
		break;
	}
	case 1: { //Light switch
		int pin;
		std::string linkedScanningGun = "";
		std::string gpioDeviceCode = "";
		auto deviceParamList = deviceConfig.at("deviceParamConfigList");
		for (auto deviceParam : deviceParamList) {
			switch (light_param_map[deviceParam["paramCode"]]) {
			case 0:
				linkedScanningGun = deviceParam["paramValue"];
				break;
			case 1:
				pin = std::stoi((std::string)deviceParam["paramValue"]);
				break;
			case 2:
				gpioDeviceCode = deviceParam["paramValue"];
				break;
			default:
				break;
			}
		}

		if (trigger_map.find(pin) == trigger_map.end()) {
			std::vector<std::string> vlinkedScanningGun;
			vlinkedScanningGun.push_back(linkedScanningGun);
			trigger_map.insert(std::make_pair(pin, vlinkedScanningGun));
		}
		else {
			trigger_map.find(pin)->second.push_back(linkedScanningGun);
		}

		if (device_map.find(gpioDeviceCode) != device_map.end()) {
			auto gpioObj = std::dynamic_pointer_cast<GPIO>(device_map.find(gpioDeviceCode)->second);
			gpioObj->addTriggerPin(pin);
		}

		auto lightSwitchObj = std::make_shared<LightSwitch>(pin, stopFlag, WM_GPIOBASEMSG, codereaderID, true, deviceGPIO, deviceTypeId, deviceTypeName, deviceTypeCode, deviceName, deviceCode);
		device_map.insert(std::make_pair(deviceCode, lightSwitchObj));
		break;
	}
	case 2: { //Camera
		if (device_map.find(deviceCode) == device_map.end()) {
			std::string ipAddr = deviceConfig.at("ipAddr");
			auto& deviceParamConfigList = deviceConfig["deviceParamConfigList"];
			auto cameraObj = Camera::create(ipAddr, deviceParamConfigList, deviceTypeId, deviceTypeName, deviceTypeCode, deviceName, deviceCode);
			if (cameraObj) {
				device_map.insert(std::make_pair(deviceCode, cameraObj));
			}
			else {
				log_error("Camera device " + deviceCode + " create failed!");
			}
		}
		break;
	}
	case 3: { //Scangun
		if (device_map.find(deviceCode) == device_map.end()) {
			std::string ipAddr = deviceConfig.at("ipAddr");
			auto& deviceParamConfigList = deviceConfig["deviceParamConfigList"];
			auto codeReaderObj = CodeReader::create(ipAddr, deviceParamConfigList, deviceTypeId, deviceTypeName, deviceTypeCode, deviceName, deviceCode);
			if (codeReaderObj) {
				device_map.insert(std::make_pair(deviceCode, codeReaderObj));
			}
			else {
				log_error("Scangun device " + deviceCode + " create failed!");
			}
		}
		break;
	}
	case 4: { //Audio equitment
		auto audioObj = std::make_shared<AudioEquipment>(deviceTypeId, deviceTypeName, deviceTypeCode, deviceName, deviceCode);
		device_map.insert(std::make_pair(deviceCode, audioObj));
		break;
	}
	case 5: { //Serial port
		std::string portName = "";
		int baudRate = 9600;
		std::string message = "";
		int devicePin = 0;
		if (deviceConfig.contains("deviceParamConfigList")) {
			auto deviceParamConfigList = deviceConfig["deviceParamConfigList"];
			for (auto deviceParamConfig : deviceParamConfigList) {
				switch (serialport_param_map[deviceParamConfig["paramCode"]]) {
				case 0:
					// devicelatency
					break;
				case 1:
					// baudRate
					baudRate = std::stoi((std::string)deviceParamConfig["paramValue"]);
					break;
				case 2:
					portName = deviceParamConfig["paramValue"];
					break;
				case 3:
					message = deviceParamConfig["paramValue"];
					break;
				case 4:
					devicePin = remoteCtrlPin = std::stoi((std::string)deviceParamConfig["paramValue"]);
				default:
					break;
				}
			}
		}
		SerialCommunication* scDevice = new SerialCommunication(sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName, portName, baudRate, message, devicePin);
		device_map.insert(std::make_pair(sdeviceCode, scDevice));
		break;
	}
	default:
		break;
	}

	return;
}

bool parseBasicConfig()
{
	std::string path = projDir.c_str();
	path.append("\\basicconfig\\basicConfig.json");

	std::ifstream configFile(path);
	if (!configFile.is_open()) {
		log_error("Open basic configuration file failed!");
		return false;
	}

	try {
		nlohmann::json jsonObj = nlohmann::json::parse(configFile);
		configFile.close();

		pipeline_code = jsonObj.at("pipelineCode");
		server_ip = jsonObj.at("serverIp");
		server_port = jsonObj.at("serverPort");

		nlohmann::json deviceConfigList = jsonObj.at("deviceConfigList");
		for (auto& deviceConfig : deviceConfigList) {
			createDevice(deviceConfig);
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		std::string errinfo = "Exception occur while parse basic config! exception msg:";
		errinfo.append(e.what());
		log_error(errinfo);
		return false;
	}

	return true;
}

void createProcessUnit(const nlohmann::json &deviceConfig, std::shared_ptr<ProcessUnit> &processUnit) {
	std::string deviceCode = deviceConfig.at("deviceCode");

	processUnit->deviceCode = deviceCode;
	processUnit->deviceTypeCode = deviceConfig.at("deviceTypeCode");
	processUnit->device = device_map.find(deviceCode)->second;
	processUnit->param = deviceConfig["deviceConfigList"];

	return;
}

void createProduct(const nlohmann::json &productConfig)
{
	std::string productName = productConfig["productName"];
	std::string productSnCode = productConfig.at("productSnCode");
	std::string productSnModel = productConfig["productSnModel"];

	auto product = std::make_shared<Product>(productSnCode, productName, productSnModel);

	nlohmann::json processConfigList = productConfig["processConfigList"];
	for (auto& processConfig : processConfigList) {
		std::string processCode = processConfig.at("processCode");
		std::string processTemplateCode = processConfig["processTemplateCode"];
		std::string processTemplateName = processConfig["processTemplateName"];
		nlohmann::json& deviceConfigList = processConfig["deviceConfigList"];

		int gpioPin = 0;
		for (auto& deviceConfig : deviceConfigList) {
			std::string deviceCode = deviceConfig.at("deviceCode");
			std::string deviceTypeCode = deviceConfig.at("deviceTypeCode");

			switch (device_type_map[deviceTypeCode]) {
			case 1: {
				auto lightObj = dynamic_pointer_cast<std::shared_ptr<Light>>(device_map.find(deviceCode)->second);
				gpioPin = lightObj->getLinkPin();
				break;
			}
			case 2:
			case 3:
			case 4:
			case 5:
			case 6: {
				auto curUnit = std::make_shared<ProcessUnit>();
				createProcessUnit(deviceConfig, curUnit);
				product->addProcessUnit(gpioPin, curUnit);
				break;
			}
			default:
				break;
			}
		}
	}

	product_map.insert(std::make_pair(productSnCode, product));
	return;
}

bool parsePipelineConfig()
{
	std::string path = projDir.c_str();
	path.append("\\pipelineconfig\\pipelineConfig.json");

	std::ifstream pipelineConfigFile(path);
	if (!pipelineConfigFile.is_open()) {
		log_error("Open pipeline configuration file failed!");
		return false;
	}

	try {
		nlohmann::json jsonObj = nlohmann::json::parse(pipelineConfigFile);
		pipelineConfigFile.close();

		nlohmann::json productConfigList = jsonObj.at("productConfigList");
		for (auto& productConfig : productConfigList) {
			createProduct(productConfig);
		}
	}
	catch (const nlohmann::json::parse_error &e) {
		std::string errinfo = "Exception occur while parse pipeline config! exception msg:";
		errinfo.append(e.what());
		log_error(errinfo);
		return false;
	}

	return true;
}

bool fetchPipelineConfig() {
	httplib::Client cli(server_ip, server_port);
	std::string path = "/api/client/pipeline/config?pipelineCode=" + pipeline_code;
	bool result = false;

	std::string configPath = projDir.c_str();
	configPath.append("\\pipelineconfig\\pipelineConfig.json");
	std::ofstream pipelineConfigFile(configPath);

	if (!pipelineConfigFile.is_open()) {
		log_error("Open pipeline configuration file failed!");
		return false;
	}

	try {
		for (int i = 0; i < 3; i++) {
			auto res = cli.Get(path);
			if (res && res->status == 200) {
				nlohmann::json jsonObj = nlohmann::json::parse(res->body);
				pipelineConfigFile << jsonObj.at("data");
				result = true;
				break;
			}
		}
	}
	catch (const nlohmann::json::parse_error &e) {
		std::string errinfo = "Exception occur while parse pipeline config! exception msg:";
		errinfo.append(e.what());
		log_error(errinfo);
	}

	pipelineConfigFile.close();

	return result;
}

void alarm(nlohmann::json &jsonobj, httplib::Response &res) {
	if (!jsonobj.contains("parameters") ||
		!jsonobj["parameters"].contains("deviceCode")) {
		res.status = 400;
		res.set_content("invalid request!", "text/plain");
		return;
	}

	std::string deviceCode = jsonobj["parameters"]["deviceCode"];
	if (device_map.find(deviceCode) == device_map.end()) {
		res.status = 404;
		res.set_content("target device not found!", "text/plain");
		return;
	}

	auto device = device_map.find(deviceCode)->second;
	if (device->getDeviceTypeCode() != "AlarmLight") {
		res.status = 404;
		res.set_content("target device type error!", "text/plain");
		return;
	}

	// TODO: alarm

	res.status = 200;
	res.set_content("success!", "text/plain");

	return;
}

static std::unordered_map<std::string, void (*)(nlohmann::json&, httplib::Response&)> notification_map = {
	{"alarm", alarm}
};

void httpServerThread(LPVOID lpParam)
{
	httplib::Server svr;

	svr.Post("/notification", [](const httplib::Request &req, httplib::Response &res) {
		nlohmann::json jsonobj = nlohmann::json::parse(req.body);

		
		if (!jsonobj.contains("function")) {
			res.status = 400;
			res.set_content("invalid request!", "text/plain");
			return;
		}

		if (notification_map.find(jsonobj["function"]) == notification_map.end()) {
			res.status = 404;
			res.set_content("unsupport function!", "text/plain");
			return;
		}

		auto function = notification_map.find(jsonobj["function"])->second;
		function(jsonobj, res);

		return;
	});

	svr.listen("0.0.0.0", 9090);
	return;
}

int main() {
	DWORD dirLen = GetCurrentDirectoryA(0, NULL);
	projDir.reserve(dirLen);
	GetCurrentDirectoryA(dirLen, &projDir[0]);

	std::string logPath = projDir.c_str();
	logPath.append("\\logs\\rotating.txt");
	log_init("AIQIForHaier", logPath, 1048576 * 50, 3);

	std::thread httpServer = std::thread(HttpServerThread);
	std::thread httpPoster = std::thread(HttpPostThread);

	if (false == parseBasicConfig()) {
		log_error("Invalid basic configuration!");
		goto err;
	}

	if (false == fetchPipelineConfig()) {
		log_error("Fetch pipeline configuration failed!");
		goto err;
	}

	if (false == parsePipelineConfig()) {
		log_error("Invalid pipeline configuration!");
		goto err;
	}

	struct gpioMsg msg;
	while (true)
	{
		GpioMessageQueue.wait(msg);
		GpioMsgProc(msg);
	}

err:
	product_map.clear();
	device_map.clear();
	log_finish();

	return 0;
}