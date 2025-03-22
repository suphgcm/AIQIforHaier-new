#include <fstream>
#include <iostream>
#include <cmath>
#include <windows.h>
#include <process.h>
#include <string>
#include <vector>
#include <filesystem>
#include <libsndfile/sndfile.h>
#include "AudioEquipment.h"
#include "Log.h"

int AudioEquipment::ReadFile(const std::string& dirPath) {
	using namespace std;
	for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
		std::string fileName = entry.path().string();
		std::string extension = entry.path().extension().string();

		if (extension == ".pcm") {
			std::ifstream file(fileName, std::ios::binary | std::ios::ate);
			if (file.is_open()) {

				std::streamsize size = file.tellg();
				file.seekg(0, std::ios::beg);

				char* buffer = new char[size];
				if (file.read(buffer, size)) {
					AudioFile audioFile = { entry.path().stem().string(), buffer, size};
					m_audioFile.push_back(audioFile);
				}

				file.close();
			}
		}
	}
	return 0;
}

int AudioEquipment::To16k(const std::string& inFileName, const std::string& outFileName) const {
	std::string curFile = inFileName;
	/*
		std::string convFile = filename;
		size_t pos = filename.rfind(".pcm");

		if (pos != std::string::npos) {
			convFile.insert(pos, "_16");
		}
	*/
	// 打开输入文件
	SF_INFO in_info; // 输入文件信息结构体
	in_info.samplerate = 48000; // 设置输入采样率为48000hz
	in_info.channels = 1; // 设置输入声道数为1
	in_info.format = SF_FORMAT_RAW | SF_FORMAT_PCM_16; // 设置输入格式为raw和16位整数
	//SNDFILE* in_file = sf_open(curFile.c_str(), SFM_READ, &in_info);
	SNDFILE* in_file = sf_open(curFile.c_str(), SFM_READ, &in_info);

	if (in_file == NULL) { // 检查打开是否成功
		printf("无法打开输入文件: %s\n", sf_strerror(in_file));
		return -1;
	}

	// 检查输入文件是否为48000hz双声道
	//if (in_info.samplerate != 48000 || in_info.channels != 2)
	//{
	//    printf("输入文件不是48000hz双声道的音频\n");
	//    sf_close(in_file); // 关闭输入文件
	//    return -1;
	//}

	// 创建输出文件
	SF_INFO out_info; // 输出文件信息结构体
	out_info.samplerate = 16000; // 设置输出采样率为16000hz
	out_info.channels = in_info.channels; // 设置输出声道数与输入相同
	out_info.format = in_info.format; // 设置输出格式与输入相同
	SNDFILE* out_file = sf_open(outFileName.c_str(), SFM_WRITE, &out_info);
	if (out_file == NULL) { // 检查创建是否成功
		printf("无法创建输出文件: %s\n", sf_strerror(out_file));
		sf_close(in_file); // 关闭输入文件
		return -1;
	}

	// 创建一个缓冲区，用于存储输入和输出数据
	int BUFFER_SIZE = 4096;
	float* buffer = (float*)malloc(BUFFER_SIZE * sizeof(float));

	// 计算每次读取和写入的帧数，根据不同的采样率进行调整
	int in_frames = BUFFER_SIZE / in_info.channels; // 输入帧数
	int out_frames = (int)(in_frames * (double)out_info.samplerate / in_info.samplerate); // 输出帧数

	while (true) { // 循环处理输入文件的数据
		int read_count = sf_readf_float(in_file, buffer, in_frames); // 从输入文件读取数据，返回实际读取的帧数
		if (read_count == 0) { // 如果读取到了文件末尾，跳出循环
			break;
		}
		if (read_count < in_frames) { // 如果读取到了最后一块数据，调整输出帧数
			out_frames = (int)(read_count * (double)out_info.samplerate / in_info.samplerate);
		}

		for (int i = 0; i < out_frames; i++) { // 遍历输出帧数
			for (int j = 0; j < out_info.channels; j++) { // 遍历输出声道数
				int index = i * out_info.channels + j; // 计算输出数据在缓冲区中的索引
				int in_index = (int)(i * (double)in_info.samplerate / out_info.samplerate) * in_info.channels + j; // 计算对应的输入数据在缓冲区中的索引
				buffer[index] = buffer[in_index]; // 将输入数据复制到输出数据
			}
		}

		sf_writef_float(out_file, buffer, out_frames); // 将输出数据写入输出文件
	}

	// 释放资源并关闭文件
	free(buffer);
	sf_close(in_file);
	sf_close(out_file);
}

void AudioEquipment::PlayAudio(WAVEFORMATEX* pFormat, std::string audioFileName) {
	HWAVEOUT hWaveOut;
	WAVEHDR WaveOutHdr;
	struct AudioFile *audioFile;
	int i = 0;
	for (i = 0; i < m_audioFile.size(); i++) {
		if (m_audioFile[i].audioFileName == audioFileName) {
			break;
		}
	}
	if (i >= m_audioFile.size())
	{
		log_error("Play audio Error, not found audio file " + audioFileName);
		return;
	}
	waveOutOpen(&hWaveOut, WAVE_MAPPER, pFormat, 0L, 0L, WAVE_FORMAT_DIRECT);

	WaveOutHdr.lpData = m_audioFile[i].fileBuffer;
	WaveOutHdr.dwBufferLength = m_audioFile[i].fileSize;
	WaveOutHdr.dwBytesRecorded = 0;
	WaveOutHdr.dwUser = 0L;
	WaveOutHdr.dwFlags = 0L;
	WaveOutHdr.dwLoops = 1;

	waveOutPrepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));

	waveOutWrite(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));

	while (waveOutUnprepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
		Sleep(100);
	}

	waveOutClose(hWaveOut);
}

int AudioEquipment::RecordAudio(WAVEFORMATEX* pFormat, int seconds, std::string recordFile) {
	WAVEHDR waveHeader;
	HWAVEIN hWaveIn;
	
	char* buffer = new char[pFormat->nAvgBytesPerSec * seconds];

	MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, pFormat, 0L, 0L, WAVE_FORMAT_DIRECT);
	if (result)
	{
		printf("Failed to open waveform input device.\n");
		return -1;
	}

	waveHeader.lpData = buffer;
	waveHeader.dwBufferLength = pFormat->nAvgBytesPerSec * seconds;
	waveHeader.dwBytesRecorded = 0;
	waveHeader.dwUser = 0L;
	waveHeader.dwFlags = 0L;
	waveHeader.dwLoops = 0L;

	result = waveInPrepareHeader(hWaveIn, &waveHeader, sizeof(WAVEHDR));
	if (result)
	{
		printf("Failed to prepare waveform header.\n");
		return -1;
	}

	result = waveInAddBuffer(hWaveIn, &waveHeader, sizeof(WAVEHDR));
	if (result)
	{
		printf("Failed to add buffer.\n");
		return -1;
	}

	result = waveInStart(hWaveIn);
	if (result)
	{
		printf("Failed to start recording.\n");
		return -1;
	}

	// Record for RECORD_TIME milliseconds
	Sleep(seconds*1000);

	// Open a .pcm file for writing
	std::ofstream outFile(recordFile.c_str(), std::ios::binary);

	// Write the buffer to the file
	outFile.write(buffer, waveHeader.dwBytesRecorded);

	outFile.close();

	waveInStop(hWaveIn);
	waveInUnprepareHeader(hWaveIn, &waveHeader, sizeof(WAVEHDR));
	waveInClose(hWaveIn);

	delete[] buffer;
	return 0;
}

