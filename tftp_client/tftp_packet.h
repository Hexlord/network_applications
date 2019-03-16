#pragma once

#include "common.h"

#include <string>
#include <inttypes.h>
#include <assert.h>

namespace tftp
{

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
};

inline string To_string(Tftp_mode mode)
{
	switch (mode)
	{
	case tftp::Tftp_mode::Netascii:
		return "netascii";
	case tftp::Tftp_mode::Octet:
		return "octet";
	}
	assert(false);
	return "";
}

enum class Tftp_error : I32
{
	Error_0 = 0,
	Error_1 = 1,
	Error_2 = 2,
	Error_3 = 3,
	Error_4 = 4,
	Error_5 = 5,
	Error_6 = 6,
	Error_7 = 7,
};

inline string To_string(Tftp_error error)
{
	switch (error)
	{
	case Tftp_error::Error_0:
		return "Not defined, see error message (if any)";
	case Tftp_error::Error_1:
		return "File not found";
	case Tftp_error::Error_2:
		return "Access violation";
	case Tftp_error::Error_3:
		return "Disk full or allocation exceeded";
	case Tftp_error::Error_4:
		return "Illegal TFTP operation";
	case Tftp_error::Error_5:
		return "Unknown transfer ID";
	case Tftp_error::Error_6:
		return "File already exists";
	case Tftp_error::Error_7:
		return "No such user";
	default:
		break;
	}
	assert(false);
	return "";
}

constexpr I32 Tftp_packet_datagram_size = 516;
constexpr I32 Tftp_packet_data_size = 512;

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
		return add((const Byte*)str.c_str(), static_cast<I32>(str.size()), false);
	}

	bool add(const Byte* data_ptr, I32 data_size, bool reverse_order = true)
	{
		assert(data_size > 0);
		if (packet_size + data_size > Tftp_packet_datagram_size) return false;
		if (reverse_order) data_ptr += data_size - 1;
		for (I32 i = 0; i < data_size; ++i)
		{
			data[packet_size] = *data_ptr;
			++packet_size;
			reverse_order ? --data_ptr : ++data_ptr;;
		}
		return true;
	}

	Byte get_byte(I32 off) const
	{
		return get(off);
	}

	Word get_word(I32 off) const
	{
		Byte result[2];
		result[1] = get(off);
		result[0] = get(off + 1);
		return *((const Word*)&result[0]);
	}

	string get_string(I32 off, I32 length) const
	{
		string result;
		for (I32 i = off; i != off + length; ++i)
		{
			result += static_cast<char>(get_byte(i));
		}
		return result;
	}

	const Byte get(I32 data_off) const
	{
		assert(data_off >= 0);
		assert(data_off + 1 <= packet_size);

		return data[data_off];
	}

	Tftp_operation get_op() const
	{
		return static_cast<Tftp_operation>(get_word(0));
	}

	I32 size() const
	{
		return packet_size;
	}

	vector<Byte> get_bytes() const
	{
		vector<Byte> result;
		result.resize(packet_size);
		for (I32 i = 0; i < packet_size; ++i)
		{
			result[i] = data[i];
		}
		return result;
	}

private:


	I32 packet_size{ 0 };
	Byte data[Tftp_packet_datagram_size]{ 0 };


};

inline string To_string(const Tftp_packet& packet)
{
	if (packet.size() < 2) return "<empty or malformad TFTP packet>";
	auto type = packet.get_op();
	string result = "Package";
	switch (type)
	{
	case tftp::Tftp_operation::Read:
		result += " with RRQ";
		break;
	case tftp::Tftp_operation::Write:
		result += " with WRQ";
		break;
	case tftp::Tftp_operation::Data:
		result += " with data (" + std::to_string(packet.size()) + " bytes)";
		result += " with package_number (" + std::to_string(static_cast<I32>(packet.get_word(2))) + ")";
		break;
	case tftp::Tftp_operation::Ack:
		result += " with ack";
		result += " with package_number (" + std::to_string(static_cast<I32>(packet.get_word(2))) + ")";
		break;
	case tftp::Tftp_operation::Error:
		if (packet.size() < 4) return "<empty or malformad TFTP packet>";
		result += " with error (" + To_string(static_cast<Tftp_error>(packet.get_word(2))) + ")";
	}
	return result;
}

/*
 *	2 bytes = opcode
 *	string	= filename
 *	1 byte	= 0
 *	string	= transfer_mode
 *	1 byte	= 0
 */
inline Tftp_packet Create_read(string file_name, Tftp_mode mode = Tftp_mode::Netascii)
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
inline Tftp_packet Create_write(string file_name, Tftp_mode mode = Tftp_mode::Netascii)
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
inline Tftp_packet Create_ack(Word packet_number)
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
inline Tftp_packet Create_data(Word block_number, const Byte* data, I32 data_size)
{
	bool good = true;

	Tftp_packet packet;
	good &= packet.add(To_word(Tftp_operation::Data));
	good &= packet.add(block_number);
	good &= packet.add(data, data_size, false);

	assert(good);

	return packet;
}

/*
 *	2 bytes = opcode
 *	2 bytes = error_code
 *	string	= message
 *	1 byte	= 0
 */
inline Tftp_packet Create_error(Word error_code, string message)
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

}