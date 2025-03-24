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
		std::cerr << "Camera " + getDeviceCode() + ": set GevSCPD failed! ret=" + std::to_string(ret);
	}

	// off auto exposure
	ret = MV_CC_SetEnumValue(handle_, "ExposureAuto", 0);
	if (ret != MV_OK) {
		std::cerr << "Camera " + getDeviceCode() + ": set ExposureAuto failed! ret=" + std::to_string(ret);
	}

	// off auto Gain
	ret = MV_CC_SetEnumValue(handle_, "GainAuto", 0);
	if (ret != MV_OK) {
		std::cerr << "Camera " + getDeviceCode() + ": set auto gain failed! ret=" + std::to_string(ret);
	}

	// off trigger mode
	ret = MV_CC_SetEnumValue(handle_, "TriggerMode", 0);
	if (ret != MV_OK) {
		std::cerr << "Camera " + getDeviceCode() + ": set trigger mode failed! ret=" + std::to_string(ret);
	}

	// continuous acquisition mode
	ret = MV_CC_SetEnumValue(handle_, "AcquisitionMode", 2);
	if (ret != MV_OK) {
		std::cerr << "Camera " + getDeviceCode() + ": set AcquisitionMode failed! ret=" + std::to_string(ret);
	}

	// pixel format
	ret = MV_CC_SetEnumValue(handle_, "PixelFormat", PixelType_Gvsp_BayerRG8);
	if (ret != MV_OK) {
		std::cerr << "Camera " + getDeviceCode() + ": set PixelFormat failed! ret=" + std::to_string(ret);
	}

	// exposure time
	ret = MV_CC_SetFloatValue(handle_, "ExposureTime", 1000);
	if (ret != MV_OK) {
		std::cerr << "Camera " + getDeviceCode() + ": Set ExposureTime failed! ret=" + std::to_string(ret);
	}

	// acquisition frame rate
	ret = MV_CC_SetFloatValue(handle_, "AcquisitionFrameRate", 30);
	if (ret != MV_OK) {
		std::cerr << "Camera " + getDeviceCode() + ": Set AcquisitionFrameRate failed! ret=" + std::to_string(ret);
	}

	// exposure gain
	ret = MV_CC_SetFloatValue(handle_, "Gain", 1);
	if (ret != MV_OK) {
		std::cerr << "Camera " + getDeviceCode() + ": Set gain failed! ret=" + std::to_string(ret);
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
	int ret = MV_OK;

	for (auto deviceParam : deviceParamConfigList) {
		switch (camera_param_map[deviceParam["paramCode"]]) {
		case 0: {
			float acquisitionFrameRate = std::stof((std::string)deviceParam["paramValue"]);
			ret = MV_CC_SetFloatValue(handle_, "AcquisitionFrameRate", acquisitionFrameRate);
			if (ret != MV_OK) {
				std::cerr << "Camera " + getDeviceCode() + ": Set acquisition frame rate fail! ret=" + std::to_string(ret);
			}
			break;
		}
		case 1: {
			float exposureTime = std::stof((std::string)deviceParam["paramValue"]);
			ret = MV_CC_SetFloatValue(handle_, "ExposureTime", exposureTime);
			if (ret != MV_OK) {
				std::cerr << "Camera " + getDeviceCode() + ": Set exposure time failed! ret=" + std::to_string(ret);
			}
			break;
		}
		case 2: {
			float gain = std::stof((std::string)deviceParam["paramValue"]);
			ret = MV_CC_SetFloatValue(handle_, "Gain", gain);
			if (ret != MV_OK) {
				std::cerr << "Camera " + getDeviceCode() + ": Set gain fail! ret=" + std::to_string(ret);
			}
			break;
		}
		case 3:
			triggerLatency_ = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 4:
			acquisitionFrameCount_ = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 5: {
			int compressionQuality = std::stoi((std::string)deviceParam["paramValue"]);
			ret = MV_CC_SetIntValueEx(handle_, "ImageCompressionQuality", compressionQuality);
			if (ret != MV_OK) {
				std::cerr << "Camera " + getDeviceCode() + ": set image compression quality failed! ret=" + std::to_string(ret);
			}
			break;
		}
		case 6:
			acquisitionFrameInterval_ = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		default:
			break;
		}
	}

	return true;
}



bool Camera::getImage() {
	int ret = MV_OK;

	for (int i = 0; i < acquisitionFrameCount_; ++i) {
		MV_FRAME_OUT stOutFrame = { 0 };

		int ret = MV_CC_GetImageBuffer(handle_, &stOutFrame, 1000);
		if (ret != MV_OK) {
			return false;
		}

		MV_SAVE_IMAGE_PARAM_EX3 to_jpeg;
		to_jpeg.pData = stOutFrame.pBufAddr;
		to_jpeg.nDataLen = stOutFrame.stFrameInfo.nFrameLen;
		to_jpeg.enPixelType = stOutFrame.stFrameInfo.enPixelType;
		to_jpeg.nWidth = stOutFrame.stFrameInfo.nWidth;
		to_jpeg.nHeight = stOutFrame.stFrameInfo.nHeight;
		// 15M
		unsigned int bufferSize = 15 * 1024 * 1024;
		to_jpeg.pImageBuffer = std::make_unique<unsigned char>(bufferSize).get(); //new unsigned char[bufferSize];
		to_jpeg.nBufferSize = bufferSize;
		to_jpeg.enImageType = MV_Image_Jpeg;
		to_jpeg.nJpgQuality = compressionQuality_;
		to_jpeg.iMethodValue = 2;

		ret = MV_CC_SaveImageEx3(handle_, &to_jpeg);
		if (ret != MV_OK) {
			MV_CC_FreeImageBuffer(handle_, &stOutFrame);
			return false;
		}

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

		MV_CC_FreeImageBuffer(handle_, &stOutFrame);

		std::this_thread::sleep_for(std::chrono::milliseconds(acquisitionFrameInterval_));
	}

	return true;
}
