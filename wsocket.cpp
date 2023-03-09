#include <iostream>
#include <string>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <fstream>
#include <map>
#include <sstream>
#include "SHA1.h"
#include "base64.hpp"

using namespace std;

#define PORT 8023

std::string response_ws(string base64, string GUID) {
    string sec = base64+GUID;
    SHA1 sh;
    sh.update(sec);

    unsigned char* buf = sh.final();

    string digest;

    for(int i = 0; i < 20; i++) {
        digest.push_back(buf[i]);
    }

    return base64::to_base64(digest);
}

typedef unsigned char byte;
typedef unsigned int  uint;

class handshake_handler {
public:
	handshake_handler(std::string headers) {
		// ignora o verbo
		headers = headers.substr(headers.find('\n')+1);

		// parsa os headers
		while(headers.size() > 3 && headers.find('\n') != std::string::npos) {
			std::string cur_line        = headers.substr(0, headers.find('\n')-1);
			std::string cur_header_type = cur_line.substr(0, cur_line.find(':'));
			std::string cur_header_info = cur_line.substr(cur_line.find(':')+2);

			this->hd[cur_header_type] 	= cur_header_info;
			headers 	    			= headers.substr(headers.find('\n')+1);
		}
	}

	std::string get_response_header() {
		string res_header = "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: WebSocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + this->get_security_check() + "\r\n\r\n";
		return res_header;
	}

	std::string get_header(std::string header) {
		return hd[header];
	}

private:
	std::map<std::string, std::string> hd;

	std::string get_security_check() {
		return response_ws(this->hd["Sec-WebSocket-Key"], "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	}
};

void client_handle(int fd) {
    std::cout << "cliente conectado" << std::endl;

	unsigned char buf[2024];
	int len = recv(fd, buf, 2024, 0);

	handshake_handler hs((char*)buf);

	string res = hs.get_response_header();

	// tratar isso aqui dps.. while sendBytes > 0 ..
	send(fd, res.data(), res.size(), 0);

    for(;;) {
        unsigned char buf[1024];
        int len = recv(fd, buf, 1024, 0);

		if(len > 0) {
			for(int i = 0; i < len; i++) {
				printf("%d ", buf[i]);
			}
			printf("\n");
		}

        //std::cout << "teste" << std::endl << std::endl;
    }
}

int main(int argc, char const* argv[]) {
	int server_fd, client_fd;
	struct sockaddr_in address;
	int opt           = 1;
	int addrlen       = sizeof(address);

	{
		// Creating socket file descriptor
		if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
			perror("socket failed");
			exit(EXIT_FAILURE);
		}

		// Forcefully attaching socket to the port 8080
		// É opcional, no entanto ajuda a reusar uma porta.. evita erros do tipo: 'a porta já está em uso'
		if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
			perror("setsockopt");
			exit(EXIT_FAILURE);
		}

		address.sin_family       = AF_INET;     // ipv4
		address.sin_addr.s_addr  = INADDR_ANY;  // ip local
		address.sin_port         = htons(PORT); // passando os bytes da porta pra 'network byte order' 

		// Forcefully attaching socket to the port 8080
		if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
			perror("bind failed");
			exit(EXIT_FAILURE);
		}

		if (listen(server_fd, 20) < 0) {
			perror("listen");
			exit(EXIT_FAILURE);
		}

		std::cout << "Server iniciado" << std::endl;
	}
	
	// ACEITA REQUISIÇÕES DE CLIENTES
    for(;;) {
        // blocking
        if ((client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            std::cout << "Erro na funcao accept" << std::endl;
        }

        std::thread tclient(client_handle, client_fd);
        tclient.detach();
    }

	return 0;
}
