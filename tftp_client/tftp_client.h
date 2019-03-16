#pragma once

#include "tftp_packet.h"

#include <chrono>
#include <vector>
#include <mutex>
#include <sstream>

namespace tftp
{

using std::istringstream;

using Mutex = std::mutex;

template <typename T>
using Lock_guard = std::lock_guard<T>;
using Mutex_guard = Lock_guard<Mutex>;

using Socket = int32_t;
using Time = uint64_t;

constexpr Time Tftp_timeout_ms = 1000;
constexpr Time Tftp_quant_ms = 25;
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
	string destination_name;
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

	bool send_package(const Package& package)
	{
		sockaddr_in target;
		target.sin_family = AF_INET;
		inet_pton(AF_INET, package.address.ip.c_str(), &target.sin_addr);
		target.sin_port = htons(package.address.port);

		vector<Byte> data = package.packet.get_bytes();

		Log("Sending package: " + To_string(package.packet));

		auto send_result = sendto(socket_descriptor, data.data(), data.size(), 0, (const sockaddr*)(&target), sizeof(target));
		if (send_result <= 0)
		{
			Err("Failed to send a package to " + To_string(package.address));
			return false;
		}
		return true;
	}

	Tftp_mode get_mode() const { return mode; }
	void set_mode(Tftp_mode new_mode)  { mode = new_mode; }

private:


	vector<Package> pull_data_packages(Word packet_number)
	{
		vector<Package> result;

		vector<Package> packages = pull_packages();
		for (auto& package : packages)
		{
			Log("Pulled package " + To_string(package.packet));

			if (package.packet.get_op() == Tftp_operation::Data &&
				package.packet.get_word(2) == packet_number)
			{
				result.push_back(package);
				continue;
			}

			Err("Unexpected package, dropping");
		}

		return result;
	}

	vector<Package> pull_ack_packages(Word packet_number)
	{
		vector<Package> result;

		vector<Package> packages = pull_packages();
		for (auto& package : packages)
		{
			Log("Pulled package " + To_string(package.packet));

			if (package.packet.get_op() == Tftp_operation::Ack &&
				package.packet.get_word(2) == packet_number)
			{
				result.push_back(package);
				continue;
			}

			Err("Unexpected package, dropping");
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

	bool execute_get(string file_name, string destination_name)
	{
		Word packet_number{ 1 };
		Log("Getting file " + file_name + " into " + destination_name);
		std::ofstream out(destination_name, std::ofstream::binary);
		size_t total_size{ 0 };

		I32 attempts = Tftp_ack_attempts;

		Package request = { server_address, Create_read(file_name, mode) };
		Package response;

		bool finished{ false };

		while (attempts > 0)
		{
			/*
			 * Send out request / acknowledge
			 */
			send_package(request);

			if (finished)
			{
				out.flush();
				return true;
			}

			Time quant_time{ 0 };
			while (quant_time < Tftp_timeout_ms)
			{
				vector<Package> packages = pull_data_packages(packet_number);
				if (packages.size() > 0)
				{
					response = packages[0];
					break;
				}

				std::this_thread::sleep_for(
					std::chrono::milliseconds(Tftp_quant_ms));
				quant_time += Tftp_quant_ms;
			}
			if (quant_time < Tftp_timeout_ms) // Answer received
			{
				attempts = Tftp_ack_attempts;
				request = { response.address, Create_ack(packet_number) };
				++packet_number;

				string block = response.packet.get_string(4, response.packet.size() - 4);
				total_size += block.size();
				out << block;

				if (response.packet.size() < Tftp_packet_datagram_size)
				{
					// Finished
					Log("File of size " + std::to_string(total_size) + " bytes received");
					finished = true;
				}
			}
			else
			{
				--attempts;
				Log("Timeout passed, resending package: " + To_string(request));
			}


		}
		return false;
	}

	bool execute_put(string file_name, string destination_name)
	{
		Byte buffer[Tftp_packet_datagram_size - 4];
		Word packet_number{ 0 };
		Log("Putting file " + file_name + " into " + destination_name);
		std::ifstream in(file_name, std::ofstream::binary);
		if (!in.good())
		{
			Err("Could not read from file " + file_name);
			return false;
		}
		I32 total_size{ 0 };
		I32 last_size{ Tftp_packet_data_size };

		I32 attempts = Tftp_ack_attempts;

		Package request = { server_address, Create_write(file_name, mode) };
		Package response;

		while (attempts > 0)
		{
			/*
			 * Send out request / data
			 */
			send_package(request);

			Time quant_time{ 0 };
			while (quant_time < Tftp_timeout_ms)
			{
				vector<Package> packages = pull_ack_packages(packet_number);
				if (packages.size() > 0)
				{
					response = packages[0];
					break;
				}

				std::this_thread::sleep_for(
					std::chrono::milliseconds(Tftp_quant_ms));
				quant_time += Tftp_quant_ms;
			}
			if (quant_time < Tftp_timeout_ms) // Answer received
			{
				if (last_size < Tftp_packet_data_size)
				{
					Log("File of size " + std::to_string(total_size) + " bytes transmitted");
					return true;
				}

				attempts = Tftp_ack_attempts;

				in.read((char*)&buffer[0], Tftp_packet_data_size);

				last_size = static_cast<I32>(in.gcount());
				++packet_number;
				request = { response.address, Create_data(packet_number, buffer, last_size) };

				total_size += last_size;
								
			}
			else
			{
				--attempts;
				Log("Timeout passed, resending package: " + To_string(request));
			}


		}
		return false;
	}

	bool execute(const Tftp_command& command)
	{
		// remove old data
		pull_packages();
		
		if (command.type == Tftp_command::Type::Get_file)
		{
			return execute_get(command.file_name, command.destination_name);
		}
		else if (command.type == Tftp_command::Type::Send_file)
		{
			return execute_put(command.file_name, command.destination_name);
		}
		else if (command.type == Tftp_command::Type::Quit)
		{
			Log("Quit command received, terminating socket");
			terminate();
			return true;
		}

		return false;
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

				Log("Received package " + To_string(package.packet));

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
		data.resize(Tftp_packet_datagram_size);
		ssize_t received = recvfrom(
			socket_descriptor,
			&data[0],
			sizeof(Byte) * Tftp_packet_datagram_size,
			0,
			(sockaddr*)&addr,
			&addr_size);

		if (received <= 0) return false;

		bool good = out.packet.add(data.data(), static_cast<I32>(received), false);
		if (!good) return false;

		out.address.ip = inet_ntop(AF_INET, &addr.sin_addr, (char*)&data[0], INET_ADDRSTRLEN);
		out.address.port = ntohs(addr.sin_port);
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

	Tftp_mode mode{ Tftp_mode::Netascii };

	Address server_address;
	Socket socket_descriptor{ 0 };

	vector<Package> packages;
	Mutex packages_mutex;
	vector<Tftp_command> commands;
	Mutex commands_mutex;


};

}

