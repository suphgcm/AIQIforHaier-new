#include <mutex>
#include <string>
#include <thread>

#include "Camera.h"

bool Camera::getCameraByIpAddress_() {
	MV_CC_DEVICE_INFO_LIST deviceList;
	memset(&deviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

	int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE, &deviceList);
	if (nRet != MV_OK) {
		return false;
	}
	if (deviceList.nDeviceNum <= 0) {
		return false;
	}

	for (unsigned int i = 0; i < deviceList.nDeviceNum; i++) {
		MV_CC_DEVICE_INFO* devInfo = deviceList.pDeviceInfo[i];
		if (devInfo == nullptr) {
			break;
		}

		if (devInfo->nTLayerType == MV_GIGE_DEVICE) {
			int nIp1 = ((devInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
			int nIp2 = ((devInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
			int nIp3 = ((devInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
			int nIp4 = (devInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
			std::string ipAddress = std::to_string(nIp1) + '.' + std::to_string(nIp2) + '.' + std::to_string(nIp3) + '.' + std::to_string(nIp4);
			if (ipAddress._Equal(ipAddr_)) {
				devInfo_ = devInfo;
				return true;
			}
		}
	}

	return false;
}

bool Camera::init_() {
	bool result = getCameraByIpAddress_();
	if (result != true) {
		std::cerr << ("Camera " + getDeviceCode() + ": enumrate device failed!");
		return false;
	}

	int ret = MV_CC_CreateHandle(&handle_, devInfo_);
	if (ret != MV_OK) {
		std::cerr << "Camera " + getDeviceCode() + ": create handle failed! ret=" + std::to_string(ret);
		return false;
	}

	ret = MV_CC_OpenDevice(handle_);
	if (ret != MV_OK) {
		std::cerr << "Camera " + getDeviceCode() + ": open device failed! ret=" + std::to_string(ret);
		return false;
	}

	// adjust optimal packet size
	if (devInfo_->nTLayerType == MV_GIGE_DEVICE) {
		int packetSize = MV_CC_GetOptimalPacketSize(handle_);
		if (packetSize > 0) {
			MV_CC_SetIntValue(handle_, "GevSCPSPacketSize", packetSize);
		}
	}

	// adjust send packet delay
	ret = MV_CC_SetIntValueEx(handle_, "GevSCPD", 8000);
	if (ret != MV_OK) {
		std::cerr << ("Camera " + getDeviceCode() + ": set GevSCPD failed! ret=" + std::to_string(ret));
	}

	// off auto exposure
	ret = MV_CC_SetEnumValue(handle_, "ExposureAuto", 0);
	if (ret != MV_OK) {
		std::cerr << ("Camera " + getDeviceCode() + ": set ExposureAuto failed! ret=" + std::to_string(ret));
	}

	// off auto Gain
	ret = MV_CC_SetEnumValue(handle_, "GainAuto", 0);
	if (ret != MV_OK) {
		std::cerr << ("Camera " + getDeviceCode() + ": set auto gain failed! ret=" + std::to_string(ret));
	}

	// off trigger mode
	ret = MV_CC_SetEnumValue(handle_, "TriggerMode", 0);
	if (ret != MV_OK) {
		std::cerr << ("Camera " + getDeviceCode() + ": set trigger mode failed! ret=" + std::to_string(ret));
	}

	// continuous acquisition mode
	ret = MV_CC_SetEnumValue(handle_, "AcquisitionMode", 2);
	if (ret != MV_OK) {
		std::cerr << ("Camera " + getDeviceCode() + ": set AcquisitionMode failed! ret=" + std::to_string(ret));
	}

	// pixel format
	ret = MV_CC_SetEnumValue(handle_, "PixelFormat", PixelType_Gvsp_BayerRG8);
	if (ret != MV_OK) {
		std::cerr << ("Camera " + getDeviceCode() + ": set PixelFormat failed! ret=" + std::to_string(ret));
	}

	// exposure time
	ret = MV_CC_SetFloatValue(handle_, "ExposureTime", 1000);
	if (ret != MV_OK) {
		std::cerr << ("Camera " + getDeviceCode() + ": Set ExposureTime failed! ret=" + std::to_string(ret));
	}

	// acquisition frame rate
	ret = MV_CC_SetFloatValue(handle_, "AcquisitionFrameRate", 30);
	if (ret != MV_OK) {
		std::cerr << ("Camera " + getDeviceCode() + ": Set AcquisitionFrameRate failed! ret=" + std::to_string(ret));
	}

	// exposure gain
	ret = MV_CC_SetFloatValue(handle_, "Gain", 1);
	if (ret != MV_OK) {
		std::cerr << ("Camera " + getDeviceCode() + ": Set gain failed! ret=" + std::to_string(ret));
	}
	return true;
}

void Camera::destroy_() {
	if (handle_ == nullptr) {
		return;
	}

    MV_CC_CloseDevice(handle_);
    MV_CC_DestroyHandle(handle_);

	handle_ = nullptr;
	devInfo_ = nullptr;
	return;
}

bool Camera::startGrabbing_() {
	if (handle_ == nullptr) {
		return false;
	}

	int nRet = MV_CC_StartGrabbing(handle_);
	if (nRet != MV_OK) {
		return false;
	}

	return true;
}

bool Camera::stopGrabbing_() {
	if (handle_ == nullptr) {
		return true;
	}

	int nRet = MV_CC_StopGrabbing(handle_);
	if (nRet != MV_OK) {
		return false;
	}

	return true;
}

bool Camera::setParamByJson(const nlohmann::json& deviceParamConfigList) {
	for (auto deviceParam : deviceParamConfigList) {
		switch (CameraParammap[deviceParam["paramCode"]]) {
		case 0:
			acquisitionFrameRate = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 1:
			exposureTime = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 2:
			gain = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 3:
			// devicelatency = deviceParam["paramValue"];
			break;
		case 4:
			acquisitionBurstFrameCount = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 5:
			compressionQuality = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 6:
			cameraInterval = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		default:
			break;
		}
	}

	if (m_isInited) {
		return SetValuesForInited(exposureTime, acquisitionFrameRate, gain, acquisitionBurstFrameCount, compressionQuality, cameraInterval);
	}
	return SetValuesForUninited(exposureTime, acquisitionFrameRate, gain, acquisitionBurstFrameCount, compressionQuality, cameraInterval);
}

bool Camera::SetValuesForInited(
	float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, int compressionQuality, int cameraInterval
) {
	log_info("Camera " + e_deviceCode + ": Start set camera parameter!");
	if (!m_isInited) {
		return false;
	}

	// 设置曝光时间
	int nRet = MV_CC_SetFloatValue(m_handle, "ExposureTime", exposureTime);
	if (nRet != MV_OK) {
		log_error("Camera " + e_deviceCode + ": Set exposure time failed! Ret=" + std::to_string(nRet));
		return false;
	}
	m_exposureTime = exposureTime;
	log_info("Camera " + e_deviceCode + ": Set exposure time success!");
	// 设置采集帧率
	nRet = MV_CC_SetFloatValue(m_handle, "AcquisitionFrameRate", acquisitionFrameRate);
	if (nRet != MV_OK) {
		log_error("Camera " + e_deviceCode + ": Set acquisition frame rate fail! Ret=" + std::to_string(nRet));
		return false;
	}
	m_acquisitionFrameRate = acquisitionFrameRate;
	log_info("Camera " + e_deviceCode + ": Set acquisition frame rate success!");
	// 设置曝光增益
	nRet = MV_CC_SetFloatValue(m_handle, "Gain", gain);
	if (nRet != MV_OK) {
		log_error("Camera " + e_deviceCode + ": Set gain fail! Ret=" + std::to_string(nRet));
		return false;
	}
	m_gain = gain;

	// 采集帧计数
	m_acquisitionBurstFrameCount = acquisitionBurstFrameCount;

	// 图片压缩质量
	m_compressionQuality = compressionQuality;

	// 拍照间隔
	m_cameraInterval = cameraInterval;
	log_info("Camera " + e_deviceCode + ": Success set camera config parameter");
	return true;
}

bool Camera::SetValuesByJson(const nlohmann::json& deviceParamConfigList) {
	float exposureTime = m_exposureTime;
	float acquisitionFrameRate = m_acquisitionFrameRate;
	float gain = m_gain;
	int acquisitionBurstFrameCount = m_acquisitionBurstFrameCount;
	int compressionQuality = m_compressionQuality;
	int cameraInterval = m_cameraInterval;

	for (auto deviceParam : deviceParamConfigList) {
		switch (CameraParammap[deviceParam["paramCode"]]) {
		case 0:
			acquisitionFrameRate = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 1:
			exposureTime = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 2:
			gain = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 3:
			// devicelatency = deviceParam["paramValue"];
			break;
		case 4:
			acquisitionBurstFrameCount = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 5:
			compressionQuality = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 6:
			cameraInterval = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		default:
			break;
		}
	}

	if (m_isInited) {
		return SetValuesForInited(exposureTime, acquisitionFrameRate, gain, acquisitionBurstFrameCount, compressionQuality, cameraInterval);
	}
	return SetValuesForUninited(exposureTime, acquisitionFrameRate, gain, acquisitionBurstFrameCount, compressionQuality, cameraInterval);
}

bool Camera::GetImage(const std::string& path, void* args) {
	if (!m_isGrabbing) {
		return false;
	}

	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

	ProcessUnit* unit = (ProcessUnit*)args;
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
	msg.type = MSG_TYPE_PICTURE;
	msg.sampleTime = milliseconds;

	for (int i = 0; i < m_acquisitionBurstFrameCount; ++i) {
		log_info("Camera " + e_deviceCode + ": Frame " + std::to_string(i) + " start!");
		MV_FRAME_OUT stOutFrame = { 0 };

		int nRet = MV_CC_GetImageBuffer(m_handle, &stOutFrame, 1000);
		if (nRet != MV_OK) {
			log_error("Camera " + e_deviceCode + ": Get image buffer failed! " + "Ret=" + std::to_string(nRet));
			return false;
		}

		// 转化图片格式为jpeg并保存在内存中,原始图片数据可能有14MB
		MV_SAVE_IMAGE_PARAM_EX3 to_jpeg;
		to_jpeg.pData = stOutFrame.pBufAddr;
		to_jpeg.nDataLen = stOutFrame.stFrameInfo.nFrameLen;
		to_jpeg.enPixelType = stOutFrame.stFrameInfo.enPixelType;
		to_jpeg.nWidth = stOutFrame.stFrameInfo.nWidth;
		to_jpeg.nHeight = stOutFrame.stFrameInfo.nHeight;
		// todo: 分配输出缓冲区，在上传线程中上传结束后会delete这一内存，预先分配15MB
		unsigned int bufferSize = 15728640;
		to_jpeg.pImageBuffer = new unsigned char[bufferSize];
		to_jpeg.nBufferSize = bufferSize;
		to_jpeg.enImageType = MV_Image_Jpeg;
		to_jpeg.nJpgQuality = m_compressionQuality; // 图片压缩质量，以后可能需要更改
		to_jpeg.iMethodValue = 2;

		nRet = MV_CC_SaveImageEx3(m_handle, &to_jpeg);
		if (nRet != MV_OK) {
			delete[] to_jpeg.pImageBuffer; // 释放
			MV_CC_FreeImageBuffer(m_handle, &stOutFrame);
			log_error("Camera " + e_deviceCode + ": translate image format to jpeg failed! " + "Ret=" + std::to_string(nRet));
			return false;
		}

		now = std::chrono::system_clock::now();
		duration = now.time_since_epoch();
		milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

		struct picture Picture;		
		Picture.imageBuffer = to_jpeg.pImageBuffer;
		Picture.imageLen = to_jpeg.nImageLen;
		Picture.sampleTime = milliseconds;
		msg.pictures.emplace_back(Picture);
/*
		std::string filePath = projDir.c_str();
		filePath.append("\\" + pipelineCode + "\\" + unit->productSn + "\\" + unit->processesCode + "\\" + std::to_string(milliseconds) + ".jpeg");
		std::filesystem::create_directories(filePath.substr(0, filePath.find_last_of('\\')));
		FILE* fp = nullptr;
		fopen_s(&fp, filePath.c_str(), "wb");
		if (fp == nullptr) {
			std::cout << "Open file failed!" << std::endl;
		}
		else {
			fwrite(to_jpeg.pImageBuffer, 1, to_jpeg.nImageLen, fp);
			fclose(fp);
		}
*/
		// 释放
		nRet = MV_CC_FreeImageBuffer(m_handle, &stOutFrame);
		if (nRet != MV_OK) {
			log_error("Camera " + e_deviceCode + ": Free image buffer failed! " + "Ret=" + std::to_string(nRet));
		}

		log_info("Camera " + e_deviceCode + ": Frame " + std::to_string(i) + " end!");

		std::this_thread::sleep_for(std::chrono::milliseconds(m_cameraInterval));
	}

	Singleton::instance().push(msg);
	log_info("push msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn + ", processesTemplateCode : " + msg.processesTemplateCode);

	return true;
}

bool Camera::GetImageX() {
	if (!m_isGrabbing) {
		return false;
	}

	MV_FRAME_OUT stOutFrame = { 0 };

	int nRet = MV_CC_GetImageBuffer(m_handle, &stOutFrame, 1000);
	if (nRet != MV_OK) {
		printf("Get Image Buffer fail! nRet [0x%x]\n", nRet);
		return false;
	}

	MV_CC_FreeImageBuffer(m_handle, &stOutFrame);

	return true;
}
