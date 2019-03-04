#pragma once

#include "../common.h"

#include <string>
#include <inttypes.h>
#include <assert.h>

namespace tftp
{

using std::string;

enum class Tftp_operation : Word
{
	Read = 1,
	Write = 2,
	Data = 3,
	Ack = 4,
	Error = 5,
};

constexpr Word To_word(Tftp_operation op)
{
	return static_cast<Word>(op);
}

enum class Tftp_mode : I32
{
	Netascii = 1,
	Octet = 2,
	Mail = 3,
};

inline string To_string(Tftp_mode mode)
{
	switch (mode)
	{
	case tftp::Tftp_mode::Netascii:
		return "netascii";
	case tftp::Tftp_mode::Octet:
		return "octet";
	case tftp::Tftp_mode::Mail:
		return "mail";
	}
	assert(false);
	return "";
}

constexpr I32 Tftp_packet_total_size = 1024;
constexpr I32 Tftp_packet_data_size = 512;

/*
 *	2 bytes = opcode
 *	string	= filename
 *	1 byte	= 0
 *	string	= transfer_mode
 *	1 byte	= 0
 */
Tftp_packet Create_read(string file_name, Tftp_mode mode = Tftp_mode::Octet)
{
	bool good = true;

	Tftp_packet packet;
	good &= packet.add(To_word(Tftp_operation::Read));
	good &= packet.add(file_name);
	good &= packet.add(Byte{ 0 });
	good &= packet.add(To_string(mode));
	good &= packet.add(Byte{ 0 });

	assert(good);

	return packet;
}

/*
 *	2 bytes = opcode
 *	string	= filename
 *	1 byte	= 0
 *	string	= transfer_mode
 *	1 byte	= 0
 */
Tftp_packet Create_write(string file_name, Tftp_mode mode = Tftp_mode::Octet)
{
	bool good = true;

	Tftp_packet packet;
	good &= packet.add(To_word(Tftp_operation::Write));
	good &= packet.add(file_name);
	good &= packet.add(Byte{ 0 });
	good &= packet.add(To_string(mode));
	good &= packet.add(Byte{ 0 });

	assert(good);

	return packet;
}

/*
 *	2 bytes = opcode
 *	2 bytes	= packet_number
 */
Tftp_packet Create_ack(Word packet_number)
{
	bool good = true;

	Tftp_packet packet;
	good &= packet.add(To_word(Tftp_operation::Ack));
	good &= packet.add(packet_number);

	assert(good);

	return packet;
}

/*
 *	2 bytes = opcode
 *	2 bytes = block_number
 *	n bytes	= data
 */
Tftp_packet Create_data(Word block_number, const Byte* data, I32 data_size)
{
	bool good = true;

	Tftp_packet packet;
	good &= packet.add(To_word(Tftp_operation::Data));
	good &= packet.add(block_number);
	good &= packet.add(data, data_size);

	assert(good);

	return packet;
}

/*
 *	2 bytes = opcode
 *	2 bytes = error_code
 *	string	= message
 *	1 byte	= 0
 */
Tftp_packet Create_error(Word error_code, string message)
{
	bool good = true;

	Tftp_packet packet;
	good &= packet.add(To_word(Tftp_operation::Error));
	good &= packet.add(error_code);
	good &= packet.add(message);
	good &= packet.add(Byte{ 0 });

	assert(good);

	return packet;
}

class Tftp_packet
{
public:
	Tftp_packet() = default;
	~Tftp_packet() = default;
	Tftp_packet(const Tftp_packet& other) = default;
		
	void clear()
	{
		packet_size = 0;
		for (auto& byte : data) byte = 0;
	}

	bool add(Byte byte)
	{
		return add(&byte, sizeof(Byte));
	}

	bool add(Word word)
	{
		return add((const Byte*)&word, sizeof(Word));
	}	

	bool add(const string& str)
	{		
		return add((const Byte*)str.c_str(), static_cast<I32>(str.size()));
	}

	bool add(const Byte* data_ptr, I32 data_size)
	{
		assert(data_size > 0);
		if (packet_size + data_size > Tftp_packet_total_size) return false;
		for (I32 i = 0; i < data_size; ++i)
		{
			data[packet_size] = *data_ptr;
			++packet_size;
			++data_ptr;
		}
		return true;
	}

	Byte get_byte(I32 off)
	{
		return *get(off, sizeof(Byte));
	}

	Word get_word(I32 off)
	{
		return *get(off, sizeof(Word));
	}

	string get_string(I32 off, I32 length)
	{
		return string((const char*)get(off, sizeof(Byte) * length));
	}

	const Byte* get(I32 data_off, I32 data_size)
	{
		assert(data_off >= 0 && data_size >= 0);
		assert(data_off + data_size <= packet_size);

		return &data[data_off];
	}

	Tftp_operation get_op()
	{
		return static_cast<Tftp_operation>(get_word(0));
	}

private:


	I32 packet_size{ 0 };
	Byte data[Tftp_packet_total_size]{ 0 };
	

};

}