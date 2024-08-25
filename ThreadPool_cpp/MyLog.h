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
	// 定义全局变量，控制日志是否保存到文件
	bool gIsSave = false;
	//默认的日志文件名
	const std::string logname = "log.txt";

	//日志级别
	enum Level
	{
		DEBUG = 0,
		INFO,
		WARNING,
		ERROR,
		FATAL
	};
	//将日志信息保存到文件中
	void saveFile(const std::string& logname, const std::string& msg)
	{
		std::ofstream out(logname, std::ios::app);//以追加模式打开日志文件
		if (!out.is_open()) {
			std::cout << "日志文件打开失败" << std::endl;
			return;
		}
		out  << msg;;		//写入日志信息
		out.close();	//关闭日志文件
		//std::cout << "写入日志成功！" << std::endl;


	}
	//获取时间戳
	std::string getTimeString()
	{
		auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

		//转为字符串
		std::stringstream ss;
		// 可以分别以不同的形式进行显示
		ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
		return ss.str();
	}
	//将日志几倍转化为字符串
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
	std::mutex mtx; //全局锁，保证日志操作的线程安全
	//日志消息输出函数
	void LogMsg(std::string filename, int line, bool issave, int level, const char* format ...)
	{
		//构造日志前缀
		std::string levelStr = levelConvetToSting(level);
		std::string time = getTimeString();
		auto threadid = std::this_thread::get_id();
		std::stringstream ss;
		ss << threadid;
		std::string threadid_str = ss.str();

		//解析可变参数
		char buffer[1024];
		va_list arg;
		va_start(arg, format);
		vsnprintf(buffer, sizeof(buffer), format, arg);
		va_end(arg);

		//构造message
		std::string message = "[" + time + "]" + "[" + levelStr + "] " + "[" + filename + "] " 
				+ "[" + std::to_string(line) + "] " +"线程ID:" +threadid_str +":" + buffer + "\n";

		std::unique_lock<std::mutex> lock(mtx);
		if (!issave)
		{
			std::cout << message;// 输出到控制台
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


