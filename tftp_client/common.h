#pragma once

#include <inttypes.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iterator>
#include <vector>
#include <thread>
#include <fstream>

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

using Word = uint16_t;
using Byte = uint8_t;
using std::string;
using std::vector;

template<typename Out>
void Split(const std::string &s, char delim, Out result) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		*(result++) = item;
	}
}

inline std::vector<std::string> Split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	Split(s, delim, std::back_inserter(elems));
	return elems;
}

inline void Log(const string& message)
{
	std::clog << message << std::endl;
}

inline void Err(const string& message)
{
	std::cerr << message << std::endl;
}