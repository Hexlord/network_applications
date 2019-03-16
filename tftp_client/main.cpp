#include "tftp_client.h"

using namespace tftp;

bool starts_with(string s, string subs)
{
	if (s.size() < subs.size()) return false;
	for (I32 i = 0, end = static_cast<I32>(subs.size()); i != end; ++i)
	{
		if (s[i] != subs[i]) return false;
	}
	return true;
}

void command_thread(Tftp_client& client)
{
	Tftp_command command;
	while (client.is_running())
	{
		string line;
		std::getline(std::cin, line);

		vector<string> tokens = Split(line, ' ');
		auto count = tokens.size();

		if (count == 1 &&
			tokens[0] == "quit")
		{
			command.type = Tftp_command::Type::Quit;
			client.order(command);
			break;
		}
		else if (count == 1 &&
			tokens[0] == "mode")
		{
			Log("Using " + To_string(client.get_mode()) + " mode");
			continue;
		}
		else if (count == 2 &&
			tokens[0] == "mode")
		{
			if (tokens[1] == "netascii")
			{
				client.set_mode(Tftp_mode::Netascii);
				Log("Using " + To_string(client.get_mode()) + " mode");
				continue;
			}
			else if (tokens[1] == "octet")
			{
				client.set_mode(Tftp_mode::Octet);
				Log("Using " + To_string(client.get_mode()) + " mode");
				continue;
			}
		}
		else if (count == 2 &&
			tokens[0] == "get")
		{
			command.type = Tftp_command::Type::Get_file;
			command.file_name = tokens[1];
			// get file name as destination
			command.destination_name = Split(command.file_name, '/').back();
			client.order(command);
			continue;
		}
		else if (count == 3 &&
			tokens[0] == "get")
		{
			command.type = Tftp_command::Type::Get_file;
			command.file_name = tokens[1];
			command.destination_name = tokens[2];
			client.order(command);
			continue;
		}
		else if (count == 2 &&
			tokens[0] == "put")
		{
			command.type = Tftp_command::Type::Send_file;
			command.file_name = tokens[1];
			// get file name as destination
			command.destination_name = Split(command.file_name, '/').back();
			client.order(command);
			continue;
		}
		else if (count == 3 &&
			tokens[0] == "put")
		{
			command.type = Tftp_command::Type::Send_file;
			command.file_name = tokens[1];
			command.destination_name = tokens[2];
			client.order(command);
			continue;
		}

		std::cout << "Commands: \nquit\nget <filename> <destination>\nput <filename> <destination>\nmode\nmode [octet, netascii]\n" << std::endl;
	}
}

int main(int argc, char* argv[])
{
	Address server;
	if (argc < 2)
	{
		std::cout << "No address specified\n";
		server.ip = "192.168.0.100";
	}
	else
	{
		server.ip = argv[1];
	}

	Tftp_client client;
	server.port = U16{ 69 };

	if (!client.connect_to_server(server))
	{
		return 1;
	}

	/*
	Package pack;
	pack.address.ip = "192.168.0.100";
	pack.address.port = 69;
	client.send_package(pack)
	*/

	Thread t1([&client]() { client.run_daemon(); });
	Thread t2([&client]() { command_thread(client); });

	t1.join();
	t2.join();

	return 0;
}