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

		if (line == "quit")
		{
			command.type = Tftp_command::Type::Quit;
			client.order(command);
			break;
		}
		else if (starts_with(line, "get "))
		{
			command.type = Tftp_command::Type::Get_file;
			command.file_name = line.substr(4);
			client.order(command);
		}
		else
		{
			std::cout << "Commands: quit, get <filename>, put <filename>" << std::endl;
		}
	}
}

int main(int argc, char* argv[])
{
	Address server;
	if (argc < 2)
	{
		std::cout << "No address specified\n";
		server.ip = "127.0.0.1";
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

	Thread t1([&client]() { client.run_daemon(); });
	Thread t2([&client]() { command_thread(client); });

	t1.join();
	t2.join();

	return 0;
}