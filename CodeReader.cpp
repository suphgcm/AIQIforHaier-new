#include <mutex>
#include "CodeReader.h"
#include <string>
#include <chrono>
#include <thread>
#include <windows.h>
#include "Log.h"

bool CodeReader::getCodeReaderByIpAddress_() {
	MV_CODEREADER_DEVICE_INFO_LIST stDeviceList;
	memset(&stDeviceList, 0, sizeof(MV_CODEREADER_DEVICE_INFO_LIST));

	int ret = MV_CODEREADER_EnumDevices(&stDeviceList, MV_CODEREADER_GIGE_DEVICE);
	if (ret != MV_CODEREADER_OK || stDeviceList.nDeviceNum <= 0) {
		return false;
	}

	MV_CODEREADER_DEVICE_INFO *pstDevInfo = nullptr;
	for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++) {
		 pstDevInfo = stDeviceList.pDeviceInfo[i];
		if (pstDevInfo == nullptr) {
			continue;
		}

		if (pstDevInfo->nTLayerType == MV_CODEREADER_GIGE_DEVICE) {
			int nIp1 = ((pstDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
			int nIp2 = ((pstDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
			int nIp3 = ((pstDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
			int nIp4 = (pstDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
			std::string ipAddress = std::to_string(nIp1) + '.' + std::to_string(nIp2) + '.' + std::to_string(nIp3) + '.' + std::to_string(nIp4);

			if (ipAddress._Equal(ipAddr_)) {
				devInfo_ = pstDevInfo;
				return true;
			}
		}
	}

	return false;
}

bool CodeReader::init_() {
	bool result = getCodeReaderByIpAddress_();
	if (result != true) {
		std::cerr << ("Scancoder " + getDeviceCode() + ": enumrate device failed!");
		return false;
	}

	int ret = MV_CODEREADER_CreateHandle(&handle_, devInfo_);
	if (ret != MV_CODEREADER_OK) {
		std::cerr << "Scancoder " + getDeviceCode() + ": Create handle failed! ret=" + std::to_string(ret);
		return false;
	}

	ret = MV_CODEREADER_OpenDevice(handle_);
	if (ret != MV_CODEREADER_OK) {
		std::cerr << "Scancoder " + getDeviceCode() + ": Open device failed! ret=" + std::to_string(ret);
		return false;
	}

	// off auto exposure
	ret = MV_CODEREADER_SetEnumValue(handle_, "ExposureAuto", 0);
	if (ret != MV_CODEREADER_OK) {
		std::cerr << "Scancoder " + getDeviceCode() + ": Set ExposureAuto fail! ret=" + std::to_string(ret);
	}

	// off auto gain
	ret = MV_CODEREADER_SetEnumValue(handle_, "GainAuto", 0);
	if (ret != MV_CODEREADER_OK) {
        std::cerr << "Scancoder " + getDeviceCode() + ": Set GainAuto fail! ret=" + std::to_string(ret);
	}

	// on trigger mode
	ret = MV_CODEREADER_SetEnumValue(handle_, "TriggerMode", MV_CODEREADER_TRIGGER_MODE_ON);
	if (ret != MV_CODEREADER_OK) {
		std::cerr << "Scancoder " + getDeviceCode() + ": Set trigger mode failed! ret=" + std::to_string(ret);
	}

	// select software trigger source
	ret = MV_CODEREADER_SetEnumValue(handle_, "TriggerSource", MV_CODEREADER_TRIGGER_SOURCE_SOFTWARE);
	if (ret != MV_CODEREADER_OK) {
		std::cerr << "Scancoder " + getDeviceCode() + ": Set software trigger source failed! ret=" + std::to_string(ret);
	}

	// exposure time
	ret = MV_CODEREADER_SetFloatValue(handle_, "ExposureTime", 1000);
	if (ret != MV_CODEREADER_OK) {
		std::cerr << "Scancoder " + getDeviceCode() + ": Set exposure time failed! ret=" + std::to_string(ret);
	}

	// acquisition frame rate
	ret = MV_CODEREADER_SetFloatValue(handle_, "AcquisitionFrameRate", 30);
	if (ret != MV_CODEREADER_OK) {
		std::cerr << "Scancoder " + getDeviceCode() + ": Set Acquisition frame rate failed! ret=" + std::to_string(ret);
	}

	// gain
	ret = MV_CODEREADER_SetFloatValue(handle_, "Gain", 1);
	if (ret != MV_CODEREADER_OK) {
		std::cerr << "Scancoder " + getDeviceCode() + ": Set gain failed! ret=" + std::to_string(ret);
	}

	return true;
}

void CodeReader::destroy_() {
	if (handle_ == nullptr) {
		return;
	}

    MV_CODEREADER_CloseDevice(handle_);
    MV_CODEREADER_DestroyHandle(handle_);

	handle_ = nullptr;
	devInfo_ = nullptr;
	return;
}

bool CodeReader::startGrabbing_() {
	if (handle_ == nullptr) {
		return false;
	}

	int ret = MV_CODEREADER_StartGrabbing(handle_);
	if (ret != MV_CODEREADER_OK) {
		return false;
	}

	return true;
}

bool CodeReader::stopGrabbing_() {
	if (handle_ == nullptr) {
		return true;
	}

	int ret = MV_CODEREADER_StopGrabbing(handle_);
	if (ret != MV_CODEREADER_OK) {
		return false;
	}

	return true;
}

bool CodeReader::setParamByJson(const nlohmann::json &deviceParamConfigList) {
	int ret = MV_CODEREADER_OK;

	for (auto deviceParam : deviceParamConfigList) {
		switch (scangun_param_map[deviceParam["paramCode"]]) {
		case 0: {
			float acquisitionFrameRate = std::stof((std::string)deviceParam["paramValue"]);
			ret = MV_CODEREADER_SetFloatValue(handle_, "AcquisitionFrameRate", acquisitionFrameRate);
			if (ret != MV_CODEREADER_OK) {
				std::cerr << "Scancoder " + getDeviceCode() + ": Set Acquisition frame rate failed! ret=" + std::to_string(ret);
			}
			break;
		}
		case 1:
			triggerLatency_ = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 2: {
			float exposureTime = std::stof((std::string)deviceParam["paramValue"]);
			ret = MV_CODEREADER_SetFloatValue(handle_, "ExposureTime", 1000);
			if (ret != MV_CODEREADER_OK) {
				std::cerr << "Scancoder " + getDeviceCode() + ": Set exposure time failed! ret=" + std::to_string(ret);
			}
			break;
		}
		case 3: {
			float gain = std::stof((std::string)deviceParam["paramValue"]);
			ret = MV_CODEREADER_SetFloatValue(handle_, "Gain", 1);
			if (ret != MV_CODEREADER_OK) {
				std::cerr << "Scancoder " + getDeviceCode() + ": Set gain failed! ret=" + std::to_string(ret);
			}
			break;
		}
		case 4: {
			int acquisitionBurstFrameCount = std::stoi((std::string)deviceParam["paramValue"]);
			ret = MV_CODEREADER_SetIntValue(handle_, "AcquisitionBurstFrameCount", acquisitionBurstFrameCount);
			if (ret != MV_CODEREADER_OK) {
				std::cerr << "Scancoder " + getDeviceCode() + ": Set acquisiton burst frame count failed! ret=" + std::to_string(ret);
			}
			break;
		}
		case 5: {
			int lightSelectorEnable = std::stoi((std::string)deviceParam["paramValue"]);
			if (lightSelectorEnable == 1) {
				ret = MV_CODEREADER_SetCommandValue(handle_, "LightingAllEnable");
			}
			else {
				ret = MV_CODEREADER_SetCommandValue(handle_, "LightingAllDisEnable");
			}

			if (ret != MV_CODEREADER_OK) {
				std::cerr << "Scancoder " + getDeviceCode() + ": Set light failed! ret=" + std::to_string(ret);
			}
			break; 
		}
		case 6: {
			int currentPosition = std::stoi((std::string)deviceParam["paramValue"]);
			ret = MV_CODEREADER_SetIntValue(handle_, "CurrentStep", currentPosition);
		    if (ret != MV_CODEREADER_OK) {
				std::cerr << "Scancoder " + getDeviceCode() + ": Set CurrentStep failed! ret=" + std::to_string(ret);
		    }
			break;
		}
		default:
			break;
		}
	}

	return true;
}

void CodeReader::getCode(std::vector<std::string> &codeVec) {
	MV_CODEREADER_IMAGE_OUT_INFO_EX2 stFrameInfo = { 0 };
	memset(&stFrameInfo, 0, sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX2));

	int ret = MV_CODEREADER_SetCommandValue(handle_, "TriggerSoftware");
	if (ret != MV_CODEREADER_OK)
	{
		std::cerr << "Scancoder " + getDeviceCode() + ": Software trigger failed! ret=" + std::to_string(ret);
		return;
	}

	ret = MV_CODEREADER_GetOneFrameTimeoutEx2(handle_, nullptr, &stFrameInfo, 1000);
	if (ret != MV_CODEREADER_OK) {
		std::cerr << "Scancoder " + getDeviceCode() + ": Get one frame failed! ret=" + std::to_string(ret);
		return;
	}

	MV_CODEREADER_RESULT_BCR_EX2 *pstBcrResult = (MV_CODEREADER_RESULT_BCR_EX2 *)stFrameInfo.UnparsedBcrList.pstCodeListEx2;

	for (unsigned int i = 0; i < pstBcrResult->nCodeNum; i++) {
		codeVec.push_back(pstBcrResult->stBcrInfoEx2[i].chCode);
	}

	return;
}
