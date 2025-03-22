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
	// �������ļ�
	SF_INFO in_info; // �����ļ���Ϣ�ṹ��
	in_info.samplerate = 48000; // �������������Ϊ48000hz
	in_info.channels = 1; // ��������������Ϊ1
	in_info.format = SF_FORMAT_RAW | SF_FORMAT_PCM_16; // ���������ʽΪraw��16λ����
	//SNDFILE* in_file = sf_open(curFile.c_str(), SFM_READ, &in_info);
	SNDFILE* in_file = sf_open(curFile.c_str(), SFM_READ, &in_info);

	if (in_file == NULL) { // �����Ƿ�ɹ�
		printf("�޷��������ļ�: %s\n", sf_strerror(in_file));
		return -1;
	}

	// ��������ļ��Ƿ�Ϊ48000hz˫����
	//if (in_info.samplerate != 48000 || in_info.channels != 2)
	//{
	//    printf("�����ļ�����48000hz˫��������Ƶ\n");
	//    sf_close(in_file); // �ر������ļ�
	//    return -1;
	//}

	// ��������ļ�
	SF_INFO out_info; // ����ļ���Ϣ�ṹ��
	out_info.samplerate = 16000; // �������������Ϊ16000hz
	out_info.channels = in_info.channels; // ���������������������ͬ
	out_info.format = in_info.format; // ���������ʽ��������ͬ
	SNDFILE* out_file = sf_open(outFileName.c_str(), SFM_WRITE, &out_info);
	if (out_file == NULL) { // ��鴴���Ƿ�ɹ�
		printf("�޷���������ļ�: %s\n", sf_strerror(out_file));
		sf_close(in_file); // �ر������ļ�
		return -1;
	}

	// ����һ�������������ڴ洢������������
	int BUFFER_SIZE = 4096;
	float* buffer = (float*)malloc(BUFFER_SIZE * sizeof(float));

	// ����ÿ�ζ�ȡ��д���֡�������ݲ�ͬ�Ĳ����ʽ��е���
	int in_frames = BUFFER_SIZE / in_info.channels; // ����֡��
	int out_frames = (int)(in_frames * (double)out_info.samplerate / in_info.samplerate); // ���֡��

	while (true) { // ѭ�����������ļ�������
		int read_count = sf_readf_float(in_file, buffer, in_frames); // �������ļ���ȡ���ݣ�����ʵ�ʶ�ȡ��֡��
		if (read_count == 0) { // �����ȡ�����ļ�ĩβ������ѭ��
			break;
		}
		if (read_count < in_frames) { // �����ȡ�������һ�����ݣ��������֡��
			out_frames = (int)(read_count * (double)out_info.samplerate / in_info.samplerate);
		}

		for (int i = 0; i < out_frames; i++) { // �������֡��
			for (int j = 0; j < out_info.channels; j++) { // �������������
				int index = i * out_info.channels + j; // ������������ڻ������е�����
				int in_index = (int)(i * (double)in_info.samplerate / out_info.samplerate) * in_info.channels + j; // �����Ӧ�����������ڻ������е�����
				buffer[index] = buffer[in_index]; // ���������ݸ��Ƶ��������
			}
		}

		sf_writef_float(out_file, buffer, out_frames); // ���������д������ļ�
	}

	// �ͷ���Դ���ر��ļ�
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

