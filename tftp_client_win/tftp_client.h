#pragma once

#include "tftp_packet.h"

namespace tftp
{

constexpr I32 Tftp_timeout_ms = 5000;

struct Address
{
	string ip;
	I32 port{ 0 };
};

enum class Tftp_error : I32
{
	Timeout = 0,
	Select = 1,
	Connection_closed = 2,
	Receive = 3,
	No_error = 4,
	Packet_unexpected = 5,
};

class Tftp_client
{
public:
	Tftp_client() = default;
	~Tftp_client() = default;
	Tftp_client(const Tftp_client& other) = delete;

private:
	Address server_address;
	I32 socket_descriptor;
	sockaddr_in client_sock_address;


};

}

