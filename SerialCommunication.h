#pragma once

#include "equnit.h"
#include <iostream>
#include <windows.h>
#include <string>

class SerialPort : public equnit {
private:
	std::string m_portName; // �������ƣ��� "COM3"
	int m_baudRate; // ������
	std::string m_message;
	int m_devicePin = 0;

	HANDLE m_hCom = nullptr; // ���
	DCB m_dcb;
	COMMTIMEOUTS m_timeOuts;

public:
	SerialPort(std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceCode, std::string deviceName,
		const std::string& portName, const int baudRate, const std::string& message, const int devicePin) :
		equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName), m_portName(portName), m_baudRate(baudRate), m_message(message), m_devicePin(devicePin) {}
	~SerialPort() { CloseSerial(); }

	bool OpenSerial(); // �򿪲����ô���
	bool CloseSerial() const; // �رմ���
	bool SendMessage(const std::string& message) const; // ������Ϣ
	bool SendMessage() const;
	bool ReceiveMessage(int length, std::string& message) const; // ������Ϣ
};
