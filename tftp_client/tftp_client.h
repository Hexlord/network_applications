#pragma once

#include "tftp_packet.h"

#include <chrono>
#include <vector>
#include <mutex>

namespace tftp
{

using Mutex = std::mutex;

template <typename T>
using Lock_guard = std::lock_guard<T>;
using Mutex_guard = Lock_guard<Mutex>;

using Socket = int32_t;
using Time = uint64_t;

constexpr Time Tftp_timeout_ms = 1000;
constexpr Time Tftp_quant_ms = 250;
constexpr I32 Tftp_ack_attempts = 4;

struct Address
{
	string ip;
	U16 port{ 0 };
};

inline bool operator==(const Address& address, const Address& other)
{
	return address.ip == other.ip &&
		address.port == other.port;
}

inline bool operator!=(const Address& address, const Address& other)
{
	return !operator==(address, other);
}

struct Package
{
	Address address;
	Tftp_packet packet;
};

inline string To_string(Address address)
{
	return address.ip + ":" + std::to_string(address.port);
}

inline string To_string(Package package)
{
	return "Package to " + To_string(package.address) +
		" with " + To_string(package.packet);
}

enum class Tftp_client_error : I32
{
	Timeout = 0,
	Select = 1,
	Connection_closed = 2,
	Receive = 3,
	No_error = 4,
	Packet_unexpected = 5,
};



struct Tftp_command
{
	enum class Type : I32
	{
		Get_file = 0,
		Send_file = 1,
		Quit = 2,
	};
	Type type{ Type::Get_file };
	string file_name;
};

inline string To_string(const Tftp_command::Type& command_type)
{
	switch (command_type)
	{
	case Tftp_command::Type::Get_file:
		return "get file";
	case Tftp_command::Type::Send_file:
		return "send file";
	case Tftp_command::Type::Quit:
		return "quit";
		break;
	}
	assert(false);
	return "";
}

inline string To_string(const Tftp_command& command)
{
	return To_string(command.type) + " " + command.file_name;
}

class Tftp_client
{
public:
	Tftp_client() = default;
	~Tftp_client() = default;
	Tftp_client(const Tftp_client& other) = delete;

	bool connect_to_server(Address server_address)
	{
		assert(socket_descriptor == 0);
		Log("Connecting to " + To_string(server_address));

		Socket socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
		if (socket_descriptor < 0)
		{
			Err("Failed to create socket");
			return false;
		}

		I32 level{ 1 };
		I32 setsockopt_result = setsockopt(socket_descriptor, SOL_SOCKET, SO_BROADCAST, &level, sizeof(level));
		if (setsockopt_result < 0)
		{
			Err("Failed to setsockopt socket");
			close(socket_descriptor);
			return false;
		}

		Log("Connected successfully");
		this->socket_descriptor = socket_descriptor;
		this->server_address = server_address;

		running = true;
		return true;
	}

	void run_daemon()
	{
		Thread t1([this]() {listen_thread(); });
		Thread t2([this]() {execute_thread(); });

		t1.join();
		t2.join();
	}

	void order(const Tftp_command& command)
	{
		{
			Mutex_guard gate_out(commands_mutex);

			commands.push_back(command);
		}
	}

	bool is_running() const { return running; }
private:

	vector<Package> pull_data_packages(Word packet_number, Address address)
	{
		vector<Package> result;

		vector<Package> packages = pull_packages();
		for (auto& package : packages)
		{
			if (package.address == address &&
				package.packet.get_op() == Tftp_operation::Data &&
				package.packet.get_word(2) == packet_number)
			{
				result.push_back(package);
				continue;
			}

			Err("Unexpected package, dropping: " + To_string(package));
		}

		return result;
	}

	vector<Package> pull_packages()
	{
		vector<Package> result;
		{
			Mutex_guard gate_in(packages_mutex);

			result = packages;
			packages.clear();
		}
		return result;
	}

	bool execute(const Tftp_command& command)
	{
		Word packet_number{ 1 };
		if (command.type == Tftp_command::Type::Get_file)
		{
			I32 attempts = Tftp_ack_attempts;

			Package request = { server_address, Create_read(command.file_name) };
			Package response;

			while (attempts > 0)
			{
				send_package(request);

				Time quant_time{ 0 };
				while (quant_time < Tftp_timeout_ms)
				{
					vector<Package> packages;
					packages = pull_data_packages(packet_number, request.address);
					if (packages.size() > 0)
					{
						response = packages[0];
						break;
					}

					std::this_thread::sleep_for(
						std::chrono::milliseconds(Tftp_quant_ms));
					quant_time += Tftp_quant_ms;
				}
				if (quant_time < Tftp_timeout_ms) // Data found
				{
					attempts = Tftp_ack_attempts;
					++packet_number;

					string block = response.packet.get_string(4, Tftp_packet_data_size);
					Out(block, false);

					if (response.packet.size() - 4 < Tftp_packet_data_size)
					{
						// Finished
						Log("File data received");
					}
					else
					{
						request = { server_address, Create_ack(
						static_cast<Word>(static_cast<I32>(packet_number) - 1)) };
					}
				}
				else
				{
					--attempts;
					Log("Timeout passed, resending package: " + To_string(request));
				}

				
			}
			if (attempts == 0)
			{
				Err("Command out of attempts: " + To_string(command));
				return false;
			}

		}
		else if (command.type == Tftp_command::Type::Send_file)
		{
			Err("File send not implemented");
		}
		else if (command.type == Tftp_command::Type::Quit)
		{
			Log("Quit command received, terminating socket");
			terminate();
		}
		return true;
	}

	void execute_thread()
	{
		while (running)
		{
			std::this_thread::sleep_for(
				std::chrono::milliseconds(Tftp_quant_ms));

			vector<Tftp_command> commands_copy;
			{
				Mutex_guard gate_in(commands_mutex);

				commands_copy = commands;
				commands.clear();
			}
			for (auto& command : commands_copy)
			{
				if (!execute(command))
				{
					Err("Failed to execute command: " + To_string(command));
				}
			}
		}
	}

	void listen_thread()
	{
		while (running)
		{
			Address from;
			Package package;
			bool good = receive_package(package);
			if (!good) break;

			{
				Mutex_guard gate_out(packages_mutex);

				packages.push_back(package);
			}
		}

		terminate();
	}

	bool receive_package(Package& out)
	{
		out.packet.clear();
		sockaddr_in addr = { 0 };
		U32 addr_size = sizeof(addr);

		vector<Byte> data;
		data.resize(Tftp_packet_total_size);
		ssize_t received = recvfrom(
			socket_descriptor,
			&data[0],
			sizeof(Byte) * Tftp_packet_data_size,
			0,
			(sockaddr*)&addr,
			&addr_size);

		if (received <= 0) return false;

		bool good = out.packet.add(data.data(), static_cast<I32>(received));
		if (!good) return false;

		out.address.ip = inet_ntop(AF_INET, &addr.sin_addr, (char*)&data[0], INET_ADDRSTRLEN);
		out.address.port = ntohs(addr.sin_port);
		return true;
	}

	bool send_package(const Package& package)
	{
		sockaddr_in target;
		target.sin_family = AF_INET;
		inet_pton(AF_INET, package.address.ip.c_str(), &target.sin_addr);
		target.sin_port = htons(package.address.port);

		vector<Byte> data = package.packet.get_bytes();

		auto send_result = sendto(socket_descriptor, data.data(), data.size(), 0, (const sockaddr*)(&target), sizeof(target));
		if (send_result <= 0)
		{
			Err("Failed to send a package to " + To_string(package.address));
			return false;
		}
		return true;
	}

	void terminate()
	{
		if (!running) return;
		shutdown(socket_descriptor, 2);
		close(socket_descriptor);
		running = false;
	}

	bool running{ false };

	Address server_address;
	Socket socket_descriptor{ 0 };

	vector<Package> packages;
	Mutex packages_mutex;
	vector<Tftp_command> commands;
	Mutex commands_mutex;


};

}

