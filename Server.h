#ifndef SERVER_HEADER_H
#define SERVER_HEADER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <cstdio>
#include <errno.h>
#include <cstring>
#include <unistd.h>
#include <string> 
#include <iostream>
#include <vector>
#include <map>
#include <sys/epoll.h>

#include "System.h"

using std::string;
using std::vector;
using std::map;
using std::pair;

class Server_t {
public:
	Server_t() : flag_of_working(true),
		listener(-1), num(1), port(PORT), epfd(-1) { }
	~Server_t();
	bool Start();

private:
	bool flag_of_working;
	string error;
	int listener, epfd;
	struct sockaddr_in serv_addr, cli_addr;
	map<int, client_t> Data;
	vector<cach_t> Cach;
	size_t num, port;
	struct epoll_event ev, events[EPOLL_SIZE];

	bool Create_Thread();
	bool Get();
	bool End_Client(int client, char sys);
	bool Online_Client(int client);
	bool Offline_Client(int client);
	bool Send_Users(int client);
	bool Send(mess_t* buf);
	bool Send(int socket_d, int id, char sys, const char* message, size_t num, size_t pack_size);
	bool Registration(int client);
	bool Proccesing(int client);
	bool Send_to_One(mess_t* buf);
	void Print_Users();
	void Print_Cach();

	Server_t(Server_t &&) = default;
	Server_t(const Server_t &) = default;
	Server_t &operator=(Server_t &&) = default;
	Server_t &operator=(const Server_t &) = default;
};

Server_t::~Server_t() {
	printf("End\n");
}

void Server_t::Print_Users() {
	printf("\nUsers:\n");
	for (auto it : Data) {
		printf("id:%d, login:%s, status:%c\n", it.first, it.second.name.c_str(), it.second.status);
	}
}

void Server_t::Print_Cach() {
	printf("\nCach:\n");
	for (auto it : Cach) {
		printf("Client:%d ", it.client);
		print_mes(&it.mes);
	}
	printf("\n");
}

bool Server_t::Send_Users(int client) {
	try {
		auto that = Data.find(client); //redo
		for (auto it : Data) {
			if (it.first != client) {
				mess_t buf;
				bzero(&buf, sizeof(buf));
				if (that->second.status == ON) {
					if (!Send_Message(client, it.first, 'r', it.second.name.c_str(), 1, 1)) {
						throw error = "Error - Send";
					}
				}
				else {
					mess_t buf = { it.first, REG, "0", 1, 1 };
					memcpy(buf.message, it.second.name.c_str(), sizeof(buf.message));
					cach_t tmp = { buf, client };
					Cach.push_back(tmp);
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

bool Server_t::Start() {
	try {
		if (!Create_Thread()) {
			throw error = "Error - Thread";
		}
		printf("Proc:Start - Get\n");
		if (!Get()) {
			throw error = "Error - Get mes";
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Server_t::Create_Thread() {
	try {
		ev.events = EPOLLIN | EPOLLET;
		listener = socket(AF_INET, SOCK_STREAM, 0);
		if (listener == ERROR) {
			throw error = "Error - listener";
		}
		setnonblocking(listener);
		bzero((char *)&serv_addr, sizeof(serv_addr));
		bool flag = true;
		while (flag) {
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(port);
			serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
			if (bind(listener, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == ERROR) {
				if (errno == EADDRINUSE) {
					port++;
				}
				else {
					throw error = "Error - bind";
				}
			}
			else {
				flag = false;
			}
		}
		printf(">>> ip : %d:%d\n", serv_addr.sin_addr.s_addr, serv_addr.sin_port);
		printf(">>> ip : %s\n", inet_ntoa(serv_addr.sin_addr));
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Server_t::Get() {
	try {
		socklen_t socklen = sizeof(cli_addr);
		if (!listen(listener, SIZE_OF_QUEUE) == ERROR) {
			throw error = "Error - listen";
		}
		epfd = epoll_create(EPOLL_SIZE);
		if (epfd == ERROR) {
			throw error = "Error - epoll create";
		}
		printf("Epoll(fd = %d) created!\n", epfd);
		ev.data.fd = listener;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev) == ERROR) {
			throw error = "Error - epoll ctl";
		}
		printf("Main listener(%d) added to epoll!\n", epfd);

		while (flag_of_working) {
			printf("--------------------------------\nnum : %zu\n", num++);
			if (Data.size() != 0) {
				Print_Users();
			}
			if (Cach.size() != 0) {
				Print_Cach();
			}
			int epoll_events_count = epoll_wait(epfd, events, EPOLL_SIZE, EPOLL_RUN_TIMEOUT);
			if (epoll_events_count == ERROR) {
				throw error = "Error - events count";
			}
			printf("Epoll events count: %d\n", epoll_events_count);
			for (int i = 0; i < epoll_events_count; i++) {
				printf("events[%d].data.fd = %d\n", i, events[i].data.fd);
				if (events[i].data.fd == listener) {
					int client = accept(listener, (struct sockaddr *) &cli_addr, &socklen);
					if (client == ERROR) {
						throw error = "Error - accept";
					}
					if (!Registration(client)) {
						throw error = "Error - registration";
					}
				}
				else {
					if (!Proccesing(events[i].data.fd)) {
						throw error = "Error - proccesing";
					}
				}
			}
		}
		close(listener);
		close(epfd);
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Server_t::Send(mess_t* buf) {
	try {
		int notsendto = buf->id;
		for (auto it : Data) {
			if (it.second.status == ON) {
				if (it.first != notsendto) {
					if (!Send_Message(buf, it.first)) {
						throw error = "Error - Send";
					}
				}
			}
			else {
				cach_t tmp = { *buf, it.first };
				Cach.push_back(tmp);
			}
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Server_t::Send(int socket_d, int id, char sys, const char* message, size_t num, size_t pack_size) {
	try {
		for (auto it : Data) {
			if (it.second.status == ON) {
				if (it.first != socket_d) {
					if (!Send_Message(it.first, id, sys, message, num, pack_size)) {
						throw error = "Error - Send";
					}
				}
			}
			else {
				mess_t buf = { id, sys, "0", 1, 1 };
				memcpy(buf.message, message, sizeof(buf.message));
				cach_t tmp = { buf, it.first };
				Cach.push_back(tmp);
			}
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Server_t::Proccesing(int client) {
	try {
		mess_t buf;
		bzero(&buf, sizeof(buf));
		printf("Try to read from fd(%d)\n", client);
		if (recv(client, &buf, sizeof(buf), 0) == ERROR) {
			throw error = "Error - recv";
		}
		print_mes(&buf);
		switch (buf.sys) {
		case(END):
			if (!End_Client(client, buf.sys)) { throw error = "Error - End Client"; }
			break;
		case(ON):
			if (!Online_Client(client)) { throw error = "Error - Online"; }
			break;
		case(OFF):
			if (!Offline_Client(client)) { throw error = "Error - Offline"; }
			break;
		case(MES):
			if (!Send(&buf)) { throw error = "Error - Send Mes Send"; }
			break;
		case(LS):
			if (!Send_to_One(&buf)) { throw error = "Error - Send Mes Send"; }
			break;
		default:
			do_Nothing();
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Server_t::Registration(int client) {
	try {
		printf("Connection from: %s : %d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		printf("Socket assigned to : %d\n", client);
		setnonblocking(client);
		ev.data.fd = client;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev) == ERROR) {
			throw error = "Error - epoll ctl";
		}
		mess_t buf;
		bzero(&buf, sizeof(buf));
		if (recv(client, &buf, sizeof(buf), 0) == ERROR) {
			throw error = "Error - recv";
		}
		string name(buf.message);
		printf("name : %s\n", name.c_str());
		client_t tmp = { name, ON };
		Data.insert(pair<int, client_t>(client, tmp));
		printf("Add new client(fd = %d) to epoll and now buf size = %zu\n", client, Data.size());
		Print_Users();
		if (!Send_Message(client, client, REG, buf.message, 1, 1)) {
			throw error = "Error - Send Mes Reg";
		}
		if (!Send_Users(client)) {
			throw error = "Error - Send Users";
		}
		buf.id = client;
		if (!Send(&buf)) {
			throw error = "Error - Send";
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Server_t::Send_to_One(mess_t* buf) {
	try {
		string message(buf->message);
		string name = "";
		auto it = message.find(" ");
		name = message.substr(2, it - 2);
		message = message.substr(it + 1, message.size());
		for (auto it : Data) {
			if (it.second.name == name) {
				if (it.second.status == ON) {
					if (!Send_Message(it.first, buf->id, MES, message.c_str(), buf->num, buf->package_size)) {
						throw error = "Error - Send";
					}
					return true;
				}
				else {
					mess_t tmp = { buf->id, buf->sys, "0", buf->num, buf->package_size };
					memcpy(tmp.message, message.c_str(), sizeof(tmp.message));
					cach_t tmp2 = { tmp, it.first };
					Cach.push_back(tmp2);
				}
			}
		}
		if (!Send_Message(buf->id, SERVER, MES, "wrong name", 1, 1)) {
			throw error = "Error - Send";
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Server_t::Offline_Client(int client) {
	try {
		auto it = Data.find(client);
		it->second.status = OFF;
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Server_t::Online_Client(int client) {
	try {
		auto it = Data.find(client);
		if (it == Data.end()) {
			throw error = "Error - find client";
		}
		it->second.status = ON;
		bool flag = true;
		while (flag) { //redo
			auto it = Cach.begin();
			for (; it < Cach.end(); ++it) {
				if (it->client == client) {
					Send_Message(&it->mes, client);
					Cach.erase(it);
				}
			}
			if (it == Cach.begin()) {
				flag = false;
			}
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Server_t::End_Client(int client, char sys) {
	try {
		if (sys != '\0') {
			if (!Send_Message(client, SERVER, END, "1", 1, 1)) {
				throw error = "Error - Send";
			}
		}
		if (!Send(client, client, END, "0", 1, 1)) {
			throw error = "Error - send";
		}
		if (Data.erase(client) != 1) {
			throw error = "Error - del client";
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

#endif // SERVER_HEADER