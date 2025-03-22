#include "SerialCommunication.h"
#include <sstream>
#include <iomanip>

bool SerialCommunication::OpenSerial() {
	// �򿪴���
	m_hCom = CreateFileA(m_portName.c_str(), // ��������
		GENERIC_READ | GENERIC_WRITE, // �������д
		0, // ��ռ��ʽ�����ڲ�֧�ֹ���
		NULL, // ��ȫ����ָ�룬Ĭ��ֵΪNULL
		OPEN_EXISTING, // �����еĴ����ļ�
		FILE_ATTRIBUTE_NORMAL, // ͬ����ʽ������һ�����ص��ļ�
		NULL // ģ���ļ��ľ�������ڸ����ļ����ԣ�һ����ΪNULL
	);

	// �򿪴���ʧ��
	if (m_hCom == INVALID_HANDLE_VALUE) {
		DWORD dwError = GetLastError();  // ��ȡ�������
		LPVOID lpMsgBuf;

		FormatMessage(          // ���������ת��Ϊ�ɶ��Ĵ�����Ϣ
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dwError,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0,
			NULL
		);

		std::cerr << "�򿪴���ʧ�ܣ�������Ϣ : " << (LPTSTR)lpMsgBuf << std::endl;
		LocalFree(lpMsgBuf);
		return false;
	}

	// ���ô���
	GetCommState(m_hCom, &m_dcb); // ��ȡ��ǰ��������
	m_dcb.BaudRate = m_baudRate; // ������
	m_dcb.ByteSize = 8; // ����λ��8
	m_dcb.Parity = NOPARITY; // У��λ����
	m_dcb.StopBits = ONESTOPBIT; // ֹͣλ��1
	SetCommState(m_hCom, &m_dcb); // ���ô�������

	// ���ó�ʱ
	GetCommTimeouts(m_hCom, &m_timeOuts); // ��ȡ��ǰ��ʱ����
	m_timeOuts.ReadIntervalTimeout = 1000; // �������ʱ
	m_timeOuts.ReadTotalTimeoutMultiplier = 500; // ��ʱ��ϵ��
	m_timeOuts.ReadTotalTimeoutConstant = 5000; // ��ʱ�䳣��
	m_timeOuts.WriteTotalTimeoutMultiplier = 500; // дʱ��ϵ��
	m_timeOuts.WriteTotalTimeoutConstant = 2000; // дʱ�䳣��
	SetCommTimeouts(m_hCom, &m_timeOuts); // ���ó�ʱ����

	return true;
}

bool SerialCommunication::CloseSerial() const {
	if (m_hCom == INVALID_HANDLE_VALUE) {
		return true;
	}
	return CloseHandle(m_hCom);
}

bool SerialCommunication::SendMessage(const std::string& message) const {
	// ��16�����ı�ת��Ϊ�ֽڶ���
	std::stringstream ss;
	ss << std::hex << message; // ��16�����ı�д���ַ�����
	std::string byteStr;
	while (ss >> byteStr) {
		std::cout << byteStr << std::endl;
		unsigned char byte = (unsigned char)stoi(byteStr, nullptr, 16); // ���ֽ��ַ���ת��Ϊ�ֽڱ���
		bool ret = WriteFile(m_hCom, // ���ھ��
			&byte, // Ҫ���͵��ֽ�
			1, // Ҫ���͵��ֽ���
			nullptr, // ���ڽ��շ��͵��ֽ�����NULL ��ʾ������
			nullptr // �����첽������NULL ��ʾͬ������
		); 
		if (!ret) {
			return false;
		}
	}
	return true;
}

bool SerialCommunication::SendMessage() const {
	SendMessage(m_message);
	return false;
}


bool SerialCommunication::ReceiveMessage(int length, std::string& message) const {
	// ����һ���ֽڻ�����
	unsigned char buffer[1024];
	// �Ӵ��ڶ�ȡ����
	bool ret = ReadFile(m_hCom, // ���ھ��
		buffer, // ���ݻ�����
		length, // Ҫ��ȡ���ֽ���
		nullptr, // ���ڽ��ն�ȡ���ֽ�����NULL ��ʾ������
		nullptr // �����첽������NULL ��ʾͬ������
	);
	if (!ret) {
		return false;
	}

	// ���ֽڶ���ת��Ϊ16�����ı�
	std::stringstream ss;
	for (int i = 0; i < length; i++) { // �����������е��ֽ�
		ss << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i]; // ���ֽ�ת��Ϊ16�����ı���д���ַ�����
	}
	message = ss.str();
	
	return true;
}