#pragma once

#include <string>
#include <winsock2.h>

#define		PACKET_SIZE		1024

enum MESSAGE_TYPE
{
	eSendMessage,
	eLogin,
	eLogout,
	eDisconnected,
	eSetSocketID,
	eReadRoom,
	eEnterRoom,
	eLeaveRoom,
	eReadUser,
	eMAX
};

// include message info
class MessageHeader
{
public:
	MESSAGE_TYPE type;
	SOCKET client_socket;
	char senderID[32];

	MessageHeader() {}
	MessageHeader(const MESSAGE_TYPE _type, const SOCKET& _client_socket, const std::string& _senderID)
		: type(_type), client_socket(_client_socket)
	{
		strcpy_s(senderID, _senderID.c_str());
	}
};

// include message content
class Message
{
public:
	MessageHeader header;
	char buffer[PACKET_SIZE];

	Message() {}

	Message(const MessageHeader& _header, const std::string& _buffer)
		: header(_header)
	{
		strcpy_s(buffer, _buffer.c_str());
	}
};

class ClientInfo {
private:
	std::string id; // sender
	std::string room_name; // 참여 중인 방 이름
public:
	ClientInfo(const std::string& _id = "", const std::string& _room_name = "") {
		SetId(_id);
		SetRoomName(_room_name);
	}
	std::string GetId() {
		return id;
	}
	void SetId(const std::string& _id) {
		id = _id;
	}
	std::string GetRoomName() {
		return room_name;
	}
	void SetRoomName(const std::string& _room_name) {
		room_name = _room_name;
	}
};