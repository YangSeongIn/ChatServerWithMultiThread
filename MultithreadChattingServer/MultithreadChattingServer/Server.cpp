#include "Server.h"

std::vector<SOCKET> clients;
std::shared_mutex clients_mutex;

int main()
{
	Server server;
	// 들어오는 클라이언트 요청 처리
	server.AddClientThread();
	// 접속 요청 수락 - 1:N
	while (1)
	{
		SOCKET client_socket = server.Accept();
		assert(client_socket >= 0);
		// 요청 받은 클라이언트 전용 recv thread 추가
		server.AddClientReceiver(client_socket);
		MessageHeader message_header(MESSAGE_TYPE::eSetSocketID, client_socket, "");
		Message message(message_header, "");
		send(client_socket, (char*)&message, sizeof(message), 0);
	}

	// 소켓의 소멸자
	WSACleanup();

	return 0;
}

Server::Server()
{
	// 소켓 초기화 정보 저장 구조체
	WSADATA wsaData;
	// 윈도우에 어느 소켓을 활용할 것인지 알려준다. 2.2버전 WORD(unsigned short)타입
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// IPV4타입, 연결지향형 소켓, 프로토콜 TCP 사용
	server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	addr_len = sizeof(listen_address);

	listen_address = {};
	listen_address.sin_family = AF_INET;
	// htons: 빅엔디안 방식으로 데이터 변환 설정
	listen_address.sin_port = htons(PORT);
	// INADDR_ANY: 현재 컴퓨터의 IP주소로 설정. 리슨 서버이기 때문.
	// s_addr: IPV4 Internet address
	listen_address.sin_addr.s_addr = htonl(INADDR_ANY);

	int bind_success = bind(server_socket, (SOCKADDR*)&listen_address, sizeof(listen_address));
	assert(bind_success >= 0);

	// 소켓 접속 대기 상태로 변경
	// SOMAXCONN: 요청 가능한 최대 접속 승인 수
	int listen_success = listen(server_socket, SOMAXCONN);
	assert(listen_success >= 0);
}

SOCKET Server::Accept()
{
	return accept(server_socket, (SOCKADDR*)&listen_address, &addr_len);
}

void Server::AddClientReceiver(const SOCKET& client_socket)
{
	std::thread receiver(&Server::ProccessReceivedMessage, this, client_socket);
	receiver.detach();
}

void Server::AddClientThread()
{
	std::thread client_thread(&Server::ProcessReceivedClient, this);
	client_thread.detach();
}

void Server::PushMessageToQ(const Message& message)
{
	std::unique_lock<std::shared_mutex> lock(message_q_lock);
	message_q.push(message);
}

Message Server::PopMessageFromQ()
{
	std::unique_lock<std::shared_mutex> lock(message_q_lock);
	Message message = message_q.front();
	message_q.pop();

	return message;
}

bool Server::EmptyMessageQ()
{
	std::shared_lock<std::shared_mutex> lock(message_q_lock);
	return message_q.empty();
}

void Server::ProcessReceivedClient()
{
	while (1)
	{
		if (message_q.empty()) continue;
		const Message cur_message = PopMessageFromQ();
		ProcessReceivedMessage(cur_message);
	}
}

void Server::ProcessReceivedMessage(const Message& message)
{
	const MESSAGE_TYPE message_type = message.header.type;

	switch (message_type)
	{
	case MESSAGE_TYPE::eSendMessage:
		BroadcastMessage(message);
		break;
	case MESSAGE_TYPE::eSetSocketID:
		SendSocketId(message);
		break;
	case MESSAGE_TYPE::eReadRoom:
		SendRoomList(message);
		break;
	case MESSAGE_TYPE::eEnterRoom:
		EnterRoom(message);
		BroadcastMessage(Message({ {message.header}, "입장하였습니다." }));
		break;
	case MESSAGE_TYPE::eLeaveRoom:
		BroadcastMessage(Message({{message.header}, "퇴장하였습니다."}));
		LeaveRoom(message);
		break;
	case MESSAGE_TYPE::eDisconnected:
		Disconnect(message);
		break;
	case MESSAGE_TYPE::eLogin:
		Login(message);
		break;
	case MESSAGE_TYPE::eLogout:
		SendRoomList(message);
		break;
	case MESSAGE_TYPE::eReadUser:
		SendUserList(message);
		break;
	default:
		break;
	}
}

void Server::ProccessReceivedMessage(const SOCKET& client_socket)
{
	while (!WSAGetLastError())
	{
		char buffer[PACKET_SIZE] = { 0, };
		int read_byte = recv(client_socket, buffer, sizeof(buffer), 0);
		assert(read_byte > 0);

		if (read_byte == 0)
		{
			MessageHeader message_header(MESSAGE_TYPE::eDisconnected, client_socket, GetId(client_socket));
			Message message(message_header, "");
			PushMessageToQ(message);
			closesocket(client_socket);
			break;
		}

		Message received_message;
		memcpy(&received_message, buffer, sizeof(received_message));
		PushMessageToQ(received_message);
	}
}

void Server::BroadcastMessage(const Message& message)
{
	const SOCKET client_socket = message.header.client_socket;
	std::string room_name = GetRoomName(client_socket);
	for (SOCKET& client : rooms[room_name])
	{
		if (message.header.type == MESSAGE_TYPE::eSendMessage && client_socket == client)
			continue;

		send(client, (const char*)&message, sizeof(message), 0);
	}
}

void Server::RemoveClient(const SOCKET client_socket)
{
	std::unique_lock<std::shared_mutex> lock(clients_mutex);
	clients.erase(remove(clients.begin(), clients.end(), client_socket), clients.end());
}

void Server::SetRoomName(const SOCKET& client_socket, const std::string& room_name) 
{
	std::unique_lock<std::shared_mutex> lock(client_lock);
	client_infos[client_socket].SetRoomName(room_name);
}

std::string Server::GetRoomName(const SOCKET& client_socket)
{
	std::shared_lock<std::shared_mutex> lock(client_lock);
	return client_infos[client_socket].GetRoomName();
}

void Server::SetId(const SOCKET& client_socket, const std::string& _id) 
{
	std::unique_lock<std::shared_mutex> lock(client_lock);
	client_infos[client_socket].SetId(_id);
}

void Server::SendSocketId(const Message& message)
{
	send(message.header.client_socket, (const char*)&message, sizeof(message), 0);
}

std::string Server::GetId(const SOCKET& client_socket) 
{
	std::shared_lock<std::shared_mutex> lock(client_lock);
	return client_infos[client_socket].GetId();
}

void Server::EnterRoom(const Message& message) 
{
	const int client_socket = message.header.client_socket;
	const std::string room_name(message.buffer);
	SetRoomName(client_socket, room_name);

	std::unique_lock<std::shared_mutex> lock(room_lock);
	rooms[room_name].emplace_back(client_socket);
}

void Server::LeaveRoom(const Message& message)
{
	const int client_socket = message.header.client_socket;
	std::string room_name = GetRoomName(client_socket);
	std::unique_lock<std::shared_mutex> lock(room_lock);
	rooms[room_name].erase(remove(rooms[room_name].begin(), rooms[room_name].end(), client_socket), rooms[room_name].end());
	if (rooms[room_name].size() == 0) rooms.erase(room_name);
	SetRoomName(message.header.client_socket, "");
}

void Server::Login(const Message& message) 
{
	const int client_socket = message.header.client_socket;
	const std::string id(message.header.senderID);
	SetId(client_socket, id);
}

void Server::Logout(const Message& message) 
{
	const int client_socket = message.header.client_socket;
	std::unique_lock<std::shared_mutex> lock(client_lock);
	client_infos.erase(client_socket);
}

void Server::SendRoomList(const Message& message) 
{
	const SOCKET client_socket = message.header.client_socket;
	std::string lists = GetRoomList();
	Message send_message({ {MESSAGE_TYPE::eSendMessage, client_socket, "SERVER"}, lists });
	send(client_socket, (const char*)&send_message, sizeof(send_message), 0);
}

void Server::SendUserList(const Message& message)
{
	const SOCKET client_socket = message.header.client_socket;
	std::string lists = GetUserList(message);
	Message send_message({ {MESSAGE_TYPE::eSendMessage, client_socket, "SERVER"}, lists });
	send(client_socket, (const char*)&send_message, sizeof(send_message), 0);
}

void Server::Disconnect(const Message& message) 
{
	LeaveRoom(message);
	Logout(message);
}

std::string Server::GetRoomList() {
	std::shared_lock<std::shared_mutex> lock(room_lock);
	std::string lists = "\n=================\n현재 방 목록\n";
	for (auto iter = rooms.begin(); iter != rooms.end(); iter++) 
	{
		lists += iter->first + "\n";
	}
	lists += "=================\n";
	return lists;
}

std::string Server::GetUserList(const Message& message)
{
	std::shared_lock<std::shared_mutex> lock(room_lock);
	ClientInfo clientInfo = client_infos[message.header.client_socket];
	std::string lists = "\n=================\n현재 방" + clientInfo.GetRoomName() + "의 유저 목록\n";
	if (client_infos.find(message.header.client_socket) != client_infos.end())
	{
		if (clientInfo.GetRoomName() != "")
		{
			for (SOCKET& client : rooms[clientInfo.GetRoomName()])
			{			
				lists += client_infos[client].GetId() + '\n';
			}
		}
	}
	lists += "=================\n";
	return lists;
}
