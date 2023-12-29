#pragma once

#include <stdio.h>
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <cassert>
#include <vector>
#include <shared_mutex>
#include <string>
#include <queue>
#include <unordered_map>
#include "Message.h"

#pragma comment(lib, "ws2_32")

#define		PORT			4578
#define		PACKET_SIZE		1024

using		Rooms			= std::unordered_map<std::string, std::vector<SOCKET>>;
using		ClientInfos		= std::unordered_map<SOCKET, ClientInfo>;

class Server
{
public:
	Server();
	SOCKET Accept();
	void AddClientReceiver(const SOCKET& client_socket);
	void AddClientThread();

private:
	int addr_len;
	WSADATA wsa_data;
	SOCKET server_socket;
	SOCKADDR_IN listen_address;
	std::queue<Message> message_q;
	std::shared_mutex message_q_lock;
	Rooms rooms;

	// message_q
	void PushMessageToQ(const Message& message);
	Message PopMessageFromQ();
	bool EmptyMessageQ();

	// clinet Info
	ClientInfos client_infos;
	std::shared_mutex client_lock;
	std::string GetRoomName(const SOCKET& client_socket);
	void SetRoomName(const SOCKET& client_socket, const std::string& room_name);
	std::string GetId(const SOCKET& client_socket);
	void SetId(const SOCKET& client_socket, const std::string& _id);

	// Client의 socket 번호를 식별자로 활용할 수 있도록 최초 Client 연결 시 Client에게 식별자 제공.
	void SendSocketId(const Message& message);

	void ProcessReceivedClient();
	void ProcessReceivedMessage(const Message& message);
	void ProccessReceivedMessage(const SOCKET& client_sock);
	void RemoveClient(const SOCKET client_socket);
	void BroadcastMessage(const Message& message);

	// Room 및 Client 정보 관리(<방이름, 참여한 클라이언트 목록>)
	std::shared_mutex room_lock;
	void EnterRoom(const Message& message);
	void LeaveRoom(const Message& message);
	void Login(const Message& message);
	void Logout(const Message& message);
	void Disconnect(const Message& message);
	void SendRoomList(const Message& message);
	void SendUserList(const Message& message);
	std::string GetRoomList();
	std::string GetUserList(const Message& message);
};