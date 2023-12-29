#pragma once

#include <stdio.h>
#include <WinSock2.h>
#include <Ws2tcpip.h> 
#include <cstring>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <thread>
#include <cassert>
#include "Message.h"

#define		PORT			4578
#define		PACKET_SIZE		1024

class Client
{
public:
	bool is_login;
	bool is_joined_room;
	SOCKET client_socket;

	Client();

	void AddServerReceiver();
	void Login();
	void Logout();
	std::string GetUserInput();
	void GetSocketID();
	void HandleUserInput(const std::string& user_input);

private:
	WSADATA wsa_data;
	SOCKADDR_IN address;
	SOCKET socket_id = -1;
	ClientInfo client_info;

	void ReceiveFromServer();

	// ¸í·É¾î
	void RequestRoomList();
	void RequestUserList();
	void EnterRoom(const std::string& room_name);
	void LeaveRoom();
	void SendRequest(const Message& send_message);
	void SendMessages(const std::string& user_input);
	void Exit();
};

std::string& ltrim(std::string& str, std::string const& whitespace = " \r\n\t\v\f");
std::string& rtrim(std::string& str, std::string const& whitespace = " \r\n\t\v\f");
std::string& trim(std::string& str, std::string const& whitespace = " \r\n\t\v\f");
