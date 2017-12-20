#ifndef SYSTEM_HEADER_H
#define SYSTEM_HEADER_H

#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <string> 
#include <iostream>

#define SIZE 256
#define EPOLL_SIZE 1000
#define DEBUG 0

using std::string;

const int ERROR = -1;
const int SERVER = 0;
const int SUCCESS = 42;
const int PORT = 1025;
const int SIZE_OF_QUEUE = 10;
const int EPOLL_RUN_TIMEOUT = -1;

const char REG = 'r';
const char MES = 'm';
const char END = 'e';
const char ON = 'o';
const char OFF = 'w';
const char LS = 'l';

typedef struct mess {
	int id;
	char sys;
	char message[SIZE];
	size_t package_size;
	size_t num;
} mess_t;

typedef struct status {
	string name;
	char status;
} client_t;

typedef struct cach {
	mess_t mes;
	int client;
} cach_t;

int do_Nothing() {
	return SUCCESS;
}

void print_mes(mess_t* mes) {
	printf("id: %d, sys: %c, message: %s\n", mes->id, mes->sys, mes->message);
}

void print_er(string error) {
	printf("%s : %d\n", error.c_str(), errno);
}

bool IsItNumber(string word) {
	size_t i = 0;
	if (word[0] == '-') {
		i++;
	}
	while (word[i] != '\0') {
		if (!isdigit(word[i++]))
			return false;
	}
	return true;
}

int sendall(int s, char* buf, int len, int flags) {
	int total = 0;
	int n;
	while (total < len) {
		n = send(s, buf + total, len - total, flags);
		if (n == -1) {
			break;
		}
		total += n;
	}
	return (n == -1 ? -1 : total);
}

int setnonblocking(int sockfd) {
	string error;
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) == ERROR) {
		throw error = "Error - setnonblocking";
	}
	return 0;
}

bool Send_Message(int socket_d, size_t id, char sys, const char* message, size_t num, size_t pack_size) {
	try {
		string error;
		mess_t mes;
		bzero(&mes, sizeof(mes));
		memcpy(mes.message, message, sizeof(mes.message));
		mes.id = id;
		mes.sys = sys;
		mes.num = num;
		mes.package_size = pack_size;
		if (DEBUG) {
			printf("Send to %d\n", socket_d);
			printf("message : %s, id: %d, sys: %c\n", message, mes.id, mes.sys);
		}
		if (sendall(socket_d, (char*)&mes, sizeof(mes), 0) == ERROR) {
			throw error = "Error - Send Mes";
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

bool Send_Message(mess_t* buf, int socket_d) {
	try {
		string error;
		if (DEBUG) {
			printf("Send to %d\n", socket_d);
			printf("message : %s, id: %d, sys: %c\n", buf->message, buf->id, buf->sys);
		}
		if (sendall(socket_d, (char*)buf, sizeof(*buf), 0) == ERROR) {
			throw error = "Error - Send Mes";
		}
		return true;
	}
	catch (string error) {
		print_er(error);
		return false;
	}
}

void Stop(bool flag_of_working) {
	while (true) {
		string message = "";
		std::getline(std::cin, message);
		if (message == "exit") {
			flag_of_working = false;
			return;
		}
	}
}

void reverse(char s[]) {
	int i, j;
	char c;
	for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}

void itoa(int n, char s[]) {
	int i, sign;
	if ((sign = n) < 0)
		n = -n;
	i = 0;
	do {
		s[i++] = n % 10 + '0';
	} while ((n /= 10) > 0);
	if (sign < 0)
		s[i++] = '-';
	s[i] = '\0';
	reverse(s);
	return;
}

#endif // SYSTEM_HEADER