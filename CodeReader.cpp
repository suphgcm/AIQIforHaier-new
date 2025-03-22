#include <mutex>
#include "CodeReader.h"
#include <string>
#include <chrono>
#include <thread>
#include <windows.h>
#include "Log.h"

MV_CODEREADER_DEVICE_INFO* CodeReader::GetCodeReaderByIpAddress() {
	if (m_isGot) {
		return m_mvDevInfo;
	}

	MV_CODEREADER_DEVICE_INFO_LIST stDeviceList;
	memset(&stDeviceList, 0, sizeof(MV_CODEREADER_DEVICE_INFO_LIST));

	int nRet = MV_CODEREADER_EnumDevices(&stDeviceList, MV_CODEREADER_GIGE_DEVICE);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Enum codereader devices failed! ret=" + std::to_string(nRet));
		return nullptr;
	}
	if (stDeviceList.nDeviceNum <= 0) {
		log_error("Enum process doesn't found any codereader devices!");
		return nullptr;
	}
	for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++) {
		std::cout << "CodeReader[" << i << "]:" << std::endl;
		MV_CODEREADER_DEVICE_INFO* pstMVDevInfo = stDeviceList.pDeviceInfo[i];
		if (pstMVDevInfo == nullptr) {
			continue;
		}

		if (pstMVDevInfo->nTLayerType == MV_CODEREADER_GIGE_DEVICE) {
			std::string name(reinterpret_cast<const char*>(pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName), 16);
			std::cout << "UserDefinedName: " << name << std::endl;

			std::string serialNumber(reinterpret_cast<const char*>(pstMVDevInfo->SpecialInfo.stGigEInfo.chSerialNumber), 16);
			std::cout << "SerialNumber: " << serialNumber << std::endl;

			int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
			int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
			int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
			int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
			std::string ipAddress = std::to_string(nIp1) + '.' + std::to_string(nIp2) + '.' + std::to_string(nIp3) + '.' + std::to_string(nIp4);
			if (ipAddress._Equal(cIPADDR)) {
				log_info("Scancoder " + e_deviceCode + ": " + cIPADDR + " had been found successfully!");
				m_mvDevInfo = pstMVDevInfo;
				m_isGot = true;
				return pstMVDevInfo;
			}
			
		}
		else if (pstMVDevInfo->nTLayerType == MV_CODEREADER_USB_DEVICE) {
			std::cerr << "pstMVDevInfo->nTLayerType == MV_CODEREADER_USB_DEVICE, wrong!" << std::endl;
		}
		else {
			std::cerr << "pstMVDevInfo->nTLayerType does not support." << std::endl;
		}
	}

	log_error("Scancoder " + e_deviceCode + ": " + cIPADDR + " doesn't been found!");
	return NULL;
}

bool CodeReader::Init() {
	if (m_isInited) {
		return true;
	}
	if (!m_isGot) {
		std::cerr << "Code reader is not got yet." << std::endl;
		return false;
	}

	// �������
	int nRet = MV_CODEREADER_CreateHandle(&m_handle, m_mvDevInfo);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Create handle failed! ret=" + std::to_string(nRet));
		return false;
	}

	// �����豸 todo: �����м���� false ʱ������
	nRet = MV_CODEREADER_OpenDevice(m_handle);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Open device failed! ret=" + std::to_string(nRet));
		return false;
	}

	// ���ô���ģʽΪ off todo: on?
	nRet = MV_CODEREADER_SetEnumValue(m_handle, "TriggerMode", MV_CODEREADER_TRIGGER_MODE_ON);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Set trigger mode failed! ret=" + std::to_string(nRet));
		return false;
	}

	// ѡ������
	nRet = MV_CODEREADER_SetEnumValue(m_handle, "TriggerSource", MV_CODEREADER_TRIGGER_SOURCE_SOFTWARE);
	if (MV_CODEREADER_OK != nRet)
	{
		log_error("Scancoder " + e_deviceCode + ": Set software trigger source failed! ret=" + std::to_string(nRet));
		return false;
	}

/*	// ���ùر��Զ��ع�
	nRet = MV_CODEREADER_SetEnumValue(m_handle, "ExposureAuto", 0);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set ExposureAuto fail! nRet [0x%x]\n", nRet);
		return false;
	}
*/
	// �����ع�ʱ�䣬�����ع�ʱ��ǰ���ȹر��Զ��ع⣬���������ʧ��
	nRet = MV_CODEREADER_SetFloatValue(m_handle, "ExposureTime", m_exposureTime);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Set exposure time failed! ret=" + std::to_string(nRet));
	}

	// ���òɼ�֡��
	nRet = MV_CODEREADER_SetFloatValue(m_handle, "AcquisitionFrameRate", m_acquisitionFrameRate);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Set Acquisition frame rate failed! ret=" + std::to_string(nRet));
	}

	// Ӧ�ò���Ҫ�ɼ�֡�ʿ���
/*
	// �����Զ��ع�����
	nRet = MV_CODEREADER_SetEnumValue(m_handle, "GainAuto", 0);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set GainAuto fail [%x]\n", nRet);
	}
*/
	// �����ع����棬���ȹر��Զ��ع��������ʧ��
	nRet = MV_CODEREADER_SetFloatValue(m_handle, "Gain", m_gain);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Set gain failed! ret=" + std::to_string(nRet));
	}

	// ���ù�Դ
	if (m_lightSelectorEnable == 1) {
		nRet = MV_CODEREADER_SetCommandValue(m_handle, "LightingAllEnable");
	}
	else {
		nRet = MV_CODEREADER_SetCommandValue(m_handle, "LightingAllDisEnable");
	}
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Set light failed! ret=" + std::to_string(nRet));
	}

	//// ���õ������ todo: ����
	//nRet = MV_CODEREADER_SetIntValue(m_handle, "CurrentStep", m_currentPosition);
	//if (nRet != MV_CODEREADER_OK) {
	//	printf("Set CurrentStep fail! nRet [0x%x]\n", nRet);
	//}
	m_isInited = true;
	log_info("Scancoder " + e_deviceCode + ": Initiallize successfully!");
	return true;
}

bool CodeReader::Destroy() {
	if (!m_isInited) {
		return true;
	}
	if (m_isGrabbing) {
		StopGrabbing();
	}

	// �ر�����
	int nRet = MV_CODEREADER_CloseDevice(m_handle);
	if (nRet != MV_CODEREADER_OK) {
		printf("ClosDevice fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// ���پ��
	nRet = MV_CODEREADER_DestroyHandle(m_handle);
	while (nRet != MV_CODEREADER_OK) {
		printf("Destroy Handle fail! nRet [0x%x]\n", nRet);
		nRet = MV_CODEREADER_DestroyHandle(m_handle);
	}

	m_handle = nullptr;
	m_isInited = false;
	return true;
}
void* CodeReader::GetHandle() 
{
	return m_handle;
}
bool CodeReader::SetValuesForUninited(
	float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, 
	int lightSelectorEnable, int currentPosition
) {
	if (m_isInited) {
		return true;
	}

	m_exposureTime = exposureTime;
	m_acquisitionFrameRate = acquisitionFrameRate;
	m_gain = gain;
	m_acquisitionBurstFrameCount = acquisitionBurstFrameCount;
	m_lightSelectorEnable = lightSelectorEnable;
	m_currentPosition = currentPosition;

	return false;
}

bool CodeReader::SetValuesForInited(
	float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, 
	int lightSelectorEnable, int currentPosition
) {
	log_info("Scancoder " + e_deviceCode + ": Start set scancoder parameter!");
	if (!m_isInited) {
		return false;
	}

	// �����ع�ʱ��
	int nRet = MV_CODEREADER_SetFloatValue(m_handle, "ExposureTime", exposureTime);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Set ExposureTime failed! nRet=" + std::to_string(nRet));
		return false;
	}
	m_exposureTime = exposureTime;

	// ���òɼ�֡��
	nRet = MV_CODEREADER_SetFloatValue(m_handle, "AcquisitionFrameRate", acquisitionFrameRate);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Set AcquisitionFrameRate failed! nRet=" + std::to_string(nRet));
		return false;
	}
	m_acquisitionFrameRate = acquisitionFrameRate;

	// �����ع����棬���ȹر��Զ��ع��������ʧ��
	nRet = MV_CODEREADER_SetFloatValue(m_handle, "Gain", gain);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Set Gain fail! nRet=" + std::to_string(nRet));
		return false;
	}
	m_gain = gain;

	// �ɼ�֡����
	m_acquisitionBurstFrameCount = acquisitionBurstFrameCount;

	// ���ù�Դ
	if (lightSelectorEnable == 1) {
		nRet = MV_CODEREADER_SetCommandValue(m_handle, "LightingAllEnable");
	}
	else if (lightSelectorEnable == 0) {
		nRet = MV_CODEREADER_SetCommandValue(m_handle, "LightingAllDisEnable");
	}
	else {
		log_warn("Scancoder " + e_deviceCode + ": Light enable parameter is invalid");
		return false;
	}
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Set light fail! nRet=" + std::to_string(nRet));
		return false;
	}
	m_lightSelectorEnable = lightSelectorEnable;

	//// ���õ������ todo: ����
	//nRet = MV_CODEREADER_SetIntValue(m_handle, "CurrentStep", currentPosition);
	//if (nRet != MV_CODEREADER_OK) {
	//	printf("Set CurrentStep fail! nRet [0x%x]\n", nRet);
	//	// return false;
	//}
	m_currentPosition = currentPosition;
	log_info("Scancoder " + e_deviceCode + ": Set scancoder config parameter successfully");
	return true;
}

bool CodeReader::SetValuesByJson(const nlohmann::json& deviceParamConfigList) {
	float exposureTime = m_exposureTime;
	float acquisitionFrameRate = m_acquisitionFrameRate;
	float gain = m_gain;
	int acquisitionBurstFrameCount = m_acquisitionBurstFrameCount;
	int lightSelectorEnable = m_lightSelectorEnable;
	int currentPosition = m_currentPosition;

	for (auto deviceParam : deviceParamConfigList) {
		switch (ScanningGunParammap[deviceParam["paramCode"]]) {
		case 0:
			acquisitionFrameRate = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 1:
			// devicelatency = deviceParam["paramValue"];
			break;
		case 2:
			exposureTime = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 3:
			gain = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 4:
			acquisitionBurstFrameCount = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 5:
			lightSelectorEnable = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 6:
			currentPosition = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		default:
			break;
		}
	}

	if (m_isInited) {
		return SetValuesForInited(exposureTime, acquisitionFrameRate, gain, acquisitionBurstFrameCount, lightSelectorEnable, currentPosition);
	}
	return SetValuesForUninited(exposureTime, acquisitionFrameRate, gain, acquisitionBurstFrameCount, lightSelectorEnable, currentPosition);
}


bool CodeReader::StartGrabbing() {
	if (m_isGrabbing) {
		return true;
	}
	if (!m_isInited) {
		std::cerr << "Code reader is not inited yet." << std::endl;
		return false;
	}

	int nRet = MV_CODEREADER_StartGrabbing(m_handle);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Start grabbing failed! nRet=" + std::to_string(nRet));
		return false;
	}

	m_isGrabbing = true;
	return true;
}


bool CodeReader::StopGrabbing() {
	if (!m_isGrabbing) {
		return true;
	}

	int nRet = MV_CODEREADER_StopGrabbing(m_handle);
	if (nRet != MV_CODEREADER_OK) {
		log_error("Scancoder " + e_deviceCode + ": Stop grabbing failed! nRet=" + std::to_string(nRet));
		return false;
	}

	m_isGrabbing = false;
	return true;
}

int CodeReader::ReadCode(std::vector<std::string>& codes) const {
	codes.clear();
	unsigned char* pData = NULL;
	MV_CODEREADER_RESULT_BCR_EX2* stBcrResult = NULL;
	MV_CODEREADER_IMAGE_OUT_INFO_EX2 stImageInfo = { 0 };
	int nRet = MV_CODEREADER_GetOneFrameTimeoutEx2(m_handle, &pData, &stImageInfo, 50);
	if (nRet == MV_CODEREADER_OK) {

		stBcrResult = (MV_CODEREADER_RESULT_BCR_EX2*)stImageInfo.UnparsedBcrList.pstCodeListEx2;

		for (unsigned int i = 0; i < stBcrResult->nCodeNum; i++) {
			std::string log = "Scancoder " + e_deviceCode + ": Prefetch code ";
			log.append(stBcrResult->stBcrInfoEx2[i].chCode);
			log_info(log);
		}
	}

	for (int i = 0; i < m_acquisitionBurstFrameCount; ++i) {
		log_info("Scancoder " + e_deviceCode + ": Frame " + std::to_string(i) + " start!");
		memset(&stImageInfo, 0, sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX2));
		pData = NULL;

		// ����
		nRet = MV_CODEREADER_SetCommandValue(m_handle, "TriggerSoftware");
		if (MV_CODEREADER_OK != nRet)
		{
			log_error("Scancoder " + e_deviceCode + ": Set Software Once failed! nRet=" + std::to_string(nRet));
			continue;
		}

		nRet = MV_CODEREADER_GetOneFrameTimeoutEx2(m_handle, &pData, &stImageInfo, 2000);
		if (nRet != MV_CODEREADER_OK) {
			log_error("Scancoder " + e_deviceCode + ": Get one frame failed! nRet=" + std::to_string(nRet));
			continue;
		}

		stBcrResult = (MV_CODEREADER_RESULT_BCR_EX2*)stImageInfo.UnparsedBcrList.pstCodeListEx2;

		for (unsigned int i = 0; i < stBcrResult->nCodeNum; i++) {
			codes.push_back(stBcrResult->stBcrInfoEx2[i].chCode); // ����Ϊ const char * �����ַ���
		}

		log_info("Scancoder " + e_deviceCode + ": Frame " + std::to_string(i) + " end!");
		if (!codes.empty()) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::microseconds((long long)m_exposureTime));
	}

	return 0;
}

int CodeReader::GetAcquisitionBurstFrameCount() 
{
	return m_acquisitionBurstFrameCount;
}

void CodeReader::Lock() {
	m_mutex.lock();
}
void CodeReader::UnLock() {
	m_mutex.unlock();
}
