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
#include "b64_enc.cpp"

#define PORT 8051

typedef unsigned char byte;
typedef unsigned int  uint;

class handshake_handler {
public:
	handshake_handler(std::string headers) {
		// ignora o verbo
		headers = headers.substr(headers.find('\n')+1);

		// parsa os headers
		while(headers.find('\n') != std::string::npos) {
			std::string cur_line        = headers.substr(0, headers.find('\n'));
			std::string cur_header_type = cur_line.substr(0, cur_line.find(':'));
			std::string cur_header_info = cur_line.substr(cur_line.find(':')+2);

			//std::cout << cur_header_type << " " << cur_header_info << std::endl;

			this->hd[cur_header_type] 	= cur_header_info;
			headers 	    			= headers.substr(headers.find('\n')+1);
		}
	}

	std::string get_response_header() {
		string res_header = "HTTP/1.1 101 Switching Protocols\n"
        "Upgrade: websocket\n"
        "Connection: Upgrade\n"
        "Sec-WebSocket-Accept: ";
		res_header += this->get_security_check() + "\n\n";
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
	
	char buf[1024];
	int len = recv(fd, buf, 1024, 0);

	//cout << buf << endl << endl << endl;

	handshake_handler hs(buf);

	string res = hs.get_response_header();

	std::cout << res.data() << std::endl;

	send(fd, res.data(), res.size(), 0);

	// cout << "enviados: " << send(fd, res.data(), res.size(), 0) << " bytes" << std::endl;

	// while(1) {
	// 	cout << "teste" << endl;
	// 	sleep(1);
	// 	system("clear");
	// 	send(fd, res.data(), res.size(), 0);
	// 	cout << res.data() << endl;
	// }

	//std::cout << buf << std::endl << std::endl;

    for(;;) {
        char buf[1024];
        int len = recv(fd, buf, 1024, 0);

		//std::cout << len << std::endl;

        //std::cout << "teste" << std::endl << std::endl;
    }
}

// int mainx() {
// 	std::string t = "GET ws://127.0.0.1:8080/ HTTP/1.1\
// \nHost: 127.0.0.1:8080\
// \nConnection: Upgrade\
// \nPragma: no-cache\
// \nCache-Control: no-cache\
// \nUser-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\
// \nUpgrade: websocket\
// \nOrigin: http://127.0.0.1:5500\
// \nSec-WebSocket-Version: 13\
// \nAccept-Encoding: gzip, deflate, br\
// \nAccept-Language: pt-BR,pt;q=0.9,en-US;q=0.8,en;q=0.7,zh-CN;q=0.6,zh;q=0.5\
// \nSec-WebSocket-Key: mxnaTS4AD0VXItpa8MtW2w==\
// \nSec-WebSocket-Extensions: permessage-deflate; client_max_window_bits";

// 	//cout << t << endl;

// 	handshake_handler hh(t);

// 	std::cout << hh.get_response_header() << std::endl;

// 	return 0;
// }

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
