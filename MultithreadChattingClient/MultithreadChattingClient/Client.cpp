#include "Client.h"

#pragma comment(lib, "ws2_32")

#define PORT			4578
#define PACKET_SIZE		1024
#define SERVER_IP		"61.77.40.250"

int main()
{
	Client client;
	client.AddServerReceiver();

	while (!client.is_login)
	{
		client.Login();
	}

	while (1)
	{
		const std::string user_input = client.GetUserInput();
		client.HandleUserInput(user_input);
	}

	closesocket(client.client_socket);

	WSACleanup();
	return 0;
}

Client::Client()
{
	WSAStartup(MAKEWORD(2, 2), &wsa_data);

	client_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	address.sin_family = AF_INET;
	address.sin_port = htons(PORT);
	int inet_pton_success = inet_pton(AF_INET, SERVER_IP, &(address.sin_addr.s_addr));
	assert(inet_pton_success > 0);

	// ������ ���Ͽ� ���� ����
	int connect_success = connect(client_socket, (SOCKADDR*)&address, sizeof(address));
	assert(connect_success >= 0);
	GetSocketID();
}

void Client::GetSocketID()
{
	char buffer[PACKET_SIZE] = { 0, };
	while (!WSAGetLastError())
	{
		ZeroMemory(&buffer, PACKET_SIZE);
		recv(client_socket, buffer, sizeof(buffer), 0);
		Message recv_message;
		memcpy(&recv_message, buffer, sizeof(recv_message));
		if (recv_message.header.type == MESSAGE_TYPE::eSetSocketID)
		{
			SOCKET& non_const_sock_id = const_cast<SOCKET&>(socket_id);
			non_const_sock_id = recv_message.header.client_socket;
			is_login = false;
			is_joined_room = false;
			break;
		}
	}
}

void Client::AddServerReceiver()
{
	std::thread receiver(&Client::ReceiveFromServer, this);
	receiver.detach();
}

void Client::ReceiveFromServer()
{
	try
	{
		while (1)
		{
			Message buffer({ MessageHeader({MESSAGE_TYPE::eMAX, client_socket, ""}), "" });
			int read_byte = recv(client_socket, (char*)&buffer, sizeof(buffer), 0);
			assert(read_byte > 0);

			if (read_byte == 0)
			{
				std::cout << "read_byte is 0. Server Error\n";
				exit(0);
			}

			std::cout << buffer.header.senderID << ": " << buffer.buffer << std::endl;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "ReceiveFromServer: " << e.what() << std::endl;
	}
}

void Client::Login()
{
	if (is_login)
	{
		std::cout << "�̹� �α��� �Ǿ��ֽ��ϴ�." << std::endl;
		return;
	}

	std::cout << "ID�� �Է��ϼ���: ";
	std::string id = GetUserInput();

	id = trim(id);
	if (id == "") return;

	client_info.SetId(id);
	MessageHeader message_header(MESSAGE_TYPE::eLogin, socket_id, client_info.GetId());
	Message send_message(message_header, "");
	send(client_socket, (const char*)&send_message, sizeof(send_message), 0);
	is_login = true;
	std::cout << "�α��ο� �����Ͽ����ϴ�." << std::endl;
}

void Client::Logout()
{
	if (!is_login)
	{
		std::cout << "�α����� �ʿ��մϴ�." << std::endl;
		return;
	}
	if (is_joined_room) {
		std::cout << "�濡 ������ ���·� �α׾ƿ��� �� �����ϴ�." << std::endl;
		return;
	}

	MessageHeader message_header(MESSAGE_TYPE::eLogout, socket_id, client_info.GetId());
	Message send_message(message_header, "");
	send(client_socket, send_message.buffer, sizeof(send_message.buffer), 0);
	client_info.SetId("");
	is_login = false;
	std::cout << "�α׾ƿ��� �����Ͽ����ϴ�." << std::endl;
}

std::string Client::GetUserInput()
{
	std::string user_input = "";
	std::getline(std::cin, user_input);
	trim(user_input);
	
	return user_input;
}

void Client::HandleUserInput(const std::string& user_input)
{
	if (user_input == "/Login") Login();
	else if (user_input == "/Logout") Logout();
	else if (user_input.substr(0, 6) == "/Enter") EnterRoom(user_input.substr(6));
	else if (user_input == "/Leave") LeaveRoom();
	else if (user_input == "/Exit") Exit();
	else if (user_input == "/RoomList") RequestRoomList();
	else if (user_input == "/UserList") RequestUserList();
	else SendMessages(user_input);
}

void Client::RequestRoomList() 
{
	Message send_msg({ {MESSAGE_TYPE::eReadRoom, socket_id, client_info.GetId()}, "" });
	send(client_socket, (const char*)&send_msg, sizeof(send_msg), 0);
}

void Client::RequestUserList()
{
	Message send_msg({ {MESSAGE_TYPE::eReadUser, socket_id, client_info.GetId()}, "" });
	send(client_socket, (const char*)&send_msg, sizeof(send_msg), 0);
}

void Client::EnterRoom(const std::string& room_name)
{
	if (room_name == "") return;

	if (!is_login) 
	{
		std::cout << "�α����� �ʿ��մϴ�." << std::endl;
		return;
	}
	if (is_joined_room) 
	{
		std::cout << "�̹� �濡 ���� ���Դϴ�." << std::endl;
		return;
	}

	client_info.SetRoomName(room_name);
	is_joined_room = true;

	SendRequest(Message{ {MESSAGE_TYPE::eEnterRoom, socket_id, client_info.GetId()}, client_info.GetRoomName() });
}

void Client::LeaveRoom() 
{
	if (!is_login) 
	{
		std::cout << "�α����� �ʿ��մϴ�." << std::endl;
		return;
	}
	if (!is_joined_room) 
	{
		std::cout << "���� ���� ���� �����ϴ�." << std::endl;
		return;
	}

	is_joined_room = false;
	SendRequest(Message({ MESSAGE_TYPE::eLeaveRoom, socket_id, client_info.GetId() }, ""));
	client_info.SetRoomName("");
	std::cout << "�濡�� �����մϴ�." << std::endl;
}

void Client::SendRequest(const Message& send_message)
{
	send(client_socket, (const char*)&send_message, sizeof(send_message), 0);
}

void Client::SendMessages(const std::string& user_input) {
	SendRequest(Message({ MESSAGE_TYPE::eSendMessage, socket_id, client_info.GetId() }, user_input));
}

void Client::Exit() {
	SendRequest(Message({ MESSAGE_TYPE::eDisconnected, socket_id, client_info.GetId() }, ""));
	closesocket(client_socket);
	std::cout << "���α׷��� �����մϴ�." << std::endl;
	exit(0);
}

std::string& ltrim(std::string& str, std::string const& whitespace)
{
	str.erase(0, str.find_first_not_of(whitespace));
	return str;
}

std::string& rtrim(std::string& str, std::string const& whitespace)
{
	str.erase(str.find_last_not_of(whitespace) + 1);
	return str;
}

std::string& trim(std::string& str, std::string const& whitespace)
{
	return ltrim(rtrim(str, whitespace), whitespace);
}
