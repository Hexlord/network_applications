#pragma once

#include <inttypes.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

using I32 = int32_t;
using U16 = uint16_t;
using U32 = uint32_t;
using U64 = uint64_t;

using Thread = std::thread;

using Word = unsigned short;
using Byte = unsigned char;
using std::string;
using std::vector;

inline void Out(const string& message, bool add_endl = true)
{
	std::cout << message;
	if (add_endl) std::cout << std::endl;
}

inline void Log(const string& message)
{
	std::clog << message << std::endl;
}

inline void Err(const string& message)
{
	std::cerr << message << std::endl;
}