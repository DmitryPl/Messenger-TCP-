#ifndef CLIENT_HEADER_H
#define CLIENT_HEADER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string> 
#include <iostream>
#include <map>

#include "System.h"

using std::string;
using std::map;

const string EXIT = "exit";
const string ONLINE = "online";
const string OFFLINE = "offline";
const char* TEST = "test";

class Client_t {
public:
	explicit Client_t() :
		error("Error"), user_name("bad_name\n\t\r"), flag_of_working(true),
		socket_d(-1), user_id(0), port(PORT) { }
	~Client_t();
	bool Start();

private:
	string error;
	string user_name;
	size_t user_id;
	size_t port;
	bool flag_of_working;
	int socket_d;
	map<int, string> Data;
	struct sockaddr_in addr;

	bool Create_Socket();
	bool Get();
	bool Send();
	bool Print(mess_t* buf);
	bool End(mess_t* buf);
	bool New_User(mess_t* buf);
	void Exit();
	void Online();
	void Offline();
	void Send_to_One(string message);
	void Print_Users();

	Client_t(Client_t &&) = default;
	Client_t(const Client_t &) = default;
	Client_t &operator=(Client_t &&) = default;
	Client_t &operator=(const Client_t &) = default;
};

Client_t::~Client_t() {
	if (!flag_of_working) {
		close(socket_d);
	}
}

bool Client_t::Start() {
	try {
		if (!Create_Socket()) { throw error = "Error - Create Socket"; }
		switch (fork()) {
		case -1:
			throw error = "Error - fork";
		case 0:
			if (!Send()) { throw error = "Error - Send Mes"; }
			break;
		default:
			if (!Get()) { throw error = "Error - Get Mes"; }
			break;
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Client_t::Get() {
	try {
		static struct epoll_event ev, events[1];
		ev.events = EPOLLIN | EPOLLET;
		int epfd = epoll_create(1);
		if (DEBUG) {
			printf("Created epoll with fd: %d\n", epfd);
		}
		ev.data.fd = socket_d;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, socket_d, &ev) == ERROR) {
			throw error = "Error - epoll ctl";
		}
		while (flag_of_working) {
			mess_t buf;
			bzero(&buf, sizeof(buf));
			epoll_wait(epfd, events, 1, EPOLL_RUN_TIMEOUT);
			if (recv(socket_d, &buf, sizeof(buf), 0) == ERROR) {
				throw error = "Error - recv - get";
			}
			if (DEBUG) {
				print_mes(&buf);
			}
			switch (buf.sys) {
			case(MES):
				if (!Print(&buf)) { throw error = "Error - print"; }
				break;
			case(END):
				if (!End(&buf)) { throw error = "Error - end"; }
				break;
			case(REG):
				if (!New_User(&buf)) { throw error = "Error - New User"; }
				break;
			default:
				do_Nothing();
			}
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

void Client_t::Print_Users() {
	if (Data.size() != 0) {
		printf("\nUsers of this chat:\n");
		for (auto it : Data) {
			printf("id:%d, login:%s\n", it.first, it.second.c_str());
		}
		printf("\n");
	}
}

bool Client_t::New_User(mess_t* buf) {
	string name = buf->message;
	Data.insert(std::pair<int, string>(buf->id, name));
	Print_Users();
	return true;
}

bool Client_t::Send() {
	try {
		printf("\nYou can send now. Enter 'exit' to exit\n");
		string message = "";
		while (flag_of_working) {
			message = "";
			std::getline(std::cin, message);
			if (message == EXIT) {
				Online();
				Exit();
			}
			else if (message == ONLINE) {
				Online();
			}
			else if (message == OFFLINE) {
				Offline();
			}
			else if (message[0] == '-' && message[1] == '>') {
				Send_to_One(message);
			}
			else {
				if (!Send_Message(socket_d, user_id, MES, message.c_str(), 1, 1)) {
					throw error = "Error - Send message";
				}
			}
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Client_t::Create_Socket() {
	try {
		in_addr_t server_ip = 0;
		printf("Enter IP server(no)\n");
		char buffer[SIZE];
		bzero(&buffer, sizeof(buffer));
		//fgets(buffer, BUF_SIZE, stdin);
		user_name = TEST;
		if (user_name == TEST) {
			server_ip = INADDR_LOOPBACK;
			printf(">>> server ip : %d\n", INADDR_LOOPBACK);
		}
		else {
			if (IsItNumber(user_name)) {
				server_ip = std::stoi(user_name);
				printf(">>> server ip : %d\n", server_ip);
			}
			else {
				throw error = "Error - Number create";
			}
		}
		printf(">>> ip : %d:%d\n", addr.sin_addr.s_addr, addr.sin_port);
		printf(">>> ip : %s\n", inet_ntoa(addr.sin_addr));
		printf("Hello, What is your name?\n");
		std::getline(std::cin, user_name);
		if ((socket_d = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
			throw error = "Error - Socket";
		}
		bzero(&addr, sizeof(addr));
		bool flag = true;
		while (flag) {
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
			addr.sin_addr.s_addr = htonl(server_ip);
			if (connect(socket_d, (struct sockaddr*)&addr, sizeof(addr)) == ERROR) {
				if (errno == ECONNREFUSED) {
					port++;
				}
				else {
					throw error = "Error - Connect";
				}
			}
			else {
				flag = false;
			}
		}
		if (!Send_Message(socket_d, 0, REG, user_name.c_str(), 1, 1)) {
			throw error = "Error - Send reg";
		}
		mess_t buf;
		bzero(&buf, sizeof(buf));
		if (recv(socket_d, &buf, sizeof(buf), 0) == ERROR) {
			throw error = "Error - recv create";
		}
		user_id = buf.id;
		if (DEBUG) {
			printf(">>> user id : %zu\n", user_id);
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Client_t::End(mess_t* buf) {
	if (buf->id == SERVER) {
		flag_of_working = false;
	}
	else {
		auto it = Data.find(buf->id);
		printf("Exit : %s\n", it->second.c_str());
		Data.erase(buf->id);
	}
	return true;
}

bool Client_t::Print(mess_t* buf) {
	if (buf->id == user_id) {
		printf("You : %s\n", buf->message);
		return true;
	}
	if (buf->id == SERVER) {
		printf("SERVER : %s\n", buf->message);
		return true;
	}
	auto it = Data.find(buf->id);
	if (it == Data.end()) {
		error = "Error - find";
		print_er(error);
		return false;
	}
	printf("%s : %s\n", it->second.c_str(), buf->message);
	return true;
}

void Client_t::Online() {
	if (!Send_Message(socket_d, user_id, ON, "0", 1, 1)) {
		throw error = "Error - Send message";
	}
	return;
}

void Client_t::Offline() {
	if (!Send_Message(socket_d, user_id, OFF, "0", 1, 1)) {
		throw error = "Error - Send message";
	}
	return;
}

void Client_t::Exit() {
	if (!Send_Message(socket_d, user_id, END, "0", 1, 1)) {
		throw error = "Error - Send exit";
	}
	printf("EXIT\n");
	flag_of_working = false;
	return;
}

void Client_t::Send_to_One(string message) {
	if (!Send_Message(socket_d, user_id, LS, message.c_str(), 1, 1)) {
		throw error = "Error - Send message";
	}
	return;
}

#endif // CLIENT_HEADER_H