#pragma once

#include <iostream>
#include <mutex>
#include <sstream>
#include <fstream>
#include <cstdarg>
#include <chrono>
#include <iomanip>
#include <memory>
#include <thread>
#include <string>
namespace myLog 
{
	// ����ȫ�ֱ�����������־�Ƿ񱣴浽�ļ�
	bool gIsSave = false;
	//Ĭ�ϵ���־�ļ���
	const std::string logname = "log.txt";

	//��־����
	enum Level
	{
		DEBUG = 0,
		INFO,
		WARNING,
		ERROR,
		FATAL
	};
	//����־��Ϣ���浽�ļ���
	void saveFile(const std::string& logname, const std::string& msg)
	{
		std::ofstream out(logname, std::ios::app);//��׷��ģʽ����־�ļ�
		if (!out.is_open()) {
			std::cout << "��־�ļ���ʧ��" << std::endl;
			return;
		}
		out  << msg;;		//д����־��Ϣ
		out.close();	//�ر���־�ļ�
		//std::cout << "д����־�ɹ���" << std::endl;


	}
	//��ȡʱ���
	std::string getTimeString()
	{
		auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

		//תΪ�ַ���
		std::stringstream ss;
		// ���Էֱ��Բ�ͬ����ʽ������ʾ
		ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
		return ss.str();
	}
	//����־����ת��Ϊ�ַ���
	std::string levelConvetToSting(int level)
	{
		switch (level)
		{
		case DEBUG:
			return "DEBUG";
		case INFO:
			return "INFO";
		case WARNING:
			return "WARNING";
		case ERROR:
			return "ERROR";
		case FATAL:
			return "FATAL";
		default: return "Unknown";
		}
	}
	std::mutex mtx; //ȫ��������֤��־�������̰߳�ȫ
	//��־��Ϣ�������
	void LogMsg(std::string filename, int line, bool issave, int level, const char* format ...)
	{
		//������־ǰ׺
		std::string levelStr = levelConvetToSting(level);
		std::string time = getTimeString();
		auto threadid = std::this_thread::get_id();
		std::stringstream ss;
		ss << threadid;
		std::string threadid_str = ss.str();

		//�����ɱ����
		char buffer[1024];
		va_list arg;
		va_start(arg, format);
		vsnprintf(buffer, sizeof(buffer), format, arg);
		va_end(arg);

		//����message
		std::string message = "[" + time + "]" + "[" + levelStr + "] " + "[" + filename + "] " 
				+ "[" + std::to_string(line) + "] " +"�߳�ID:" +threadid_str +":" + buffer + "\n";

		std::unique_lock<std::mutex> lock(mtx);
		if (!issave)
		{
			std::cout << message;// ���������̨
		}
		else
		{
			saveFile(logname, message);
		}
	}

#define LOG(level, msg, ...) do { \
    LogMsg(__FILE__, __LINE__, gIsSave, level,msg, ##__VA_ARGS__); \
} while (0)

#define EnableFile() do { gIsSave = true; } while (0)
#define EnableScreen() do { gIsSave = false; } while (0)

}


