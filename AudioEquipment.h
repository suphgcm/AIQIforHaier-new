#pragma once

#include "equnit.h"
#include <string>
#include <mmsystem.h>

struct AudioFile {
	std::string audioFileName;
	char* fileBuffer;
	long fileSize;
};

// 声音播放和录制
class AudioEquipment :
	public equnit 
{
public:
	typedef short SAMPLE;
	static const int SAMPLING_RATE = 48000;
	static const int SAMPLING_RATE_16K = 16000;
	static const int CHANNEL_COUNT = 1;
	static const int CHANNEL_RECORD = 1;
	static const int BIT_DEPTH = 16;
	static const int NFRAMES_PER_BLOCK = 4096;
	static const int NUM_WRITES_PER_BUFFER = 4;

private:
	std::string m_deviceName = "Yamaha Steinberg USB ASIO";
	std::vector<struct AudioFile> m_audioFile;


public:
	AudioEquipment(std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceCode, std::string deviceName) :
		equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName) {}
	~AudioEquipment() {
		/*
		if (m_fileBuffer != nullptr) {
			delete[] m_fileBuffer;
		}
		*/
		for (auto it = m_audioFile.begin(); it != m_audioFile.end(); it++) {
			delete[] it->fileBuffer;
		}
	}


	int ReadFile(const std::string& fileName); // 读取预设音频文件
	int To16k(const std::string& inFileName, const std::string& outFileName) const;
	void PlayAudio(WAVEFORMATEX* pFormat, std::string audioFileName);
	int RecordAudio(WAVEFORMATEX* pFormat, int seconds, std::string recordFile);
};