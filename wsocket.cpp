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

std::string genAcceptHeader(string base64, string GUID) {
    SHA1 sh;

    sh.update(base64+GUID);

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

			this->header[cur_header_type] 	= cur_header_info;
			headers 	    			    = headers.substr(headers.find('\n')+1);
		}
	}

	std::string getResponseHeader() {
		string res_header = "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: WebSocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + this->getSecurityCheck() + "\r\n\r\n";
		return res_header;
	}

	std::string getHeader(std::string h) {
		return header[h];
	}

private:
	std::map<std::string, std::string> header;

	std::string getSecurityCheck() {
		return genAcceptHeader(this->header["Sec-WebSocket-Key"], "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	}
};

class PartialHeader {
public:
	unsigned char     opcode;          // opcode..
	bool  		      isMask;          // máscara
	unsigned char     payloadType;     // tipo referente ao tamanho do dado da aplicação
	unsigned char     FIN;             // usado pra verificar se existem múltiplos frames referente a um mesmo dado
	unsigned long int appDataLen = 0;  // tamanho do dado da aplicação
	unsigned char  	  mask[4];         // máscara
	unsigned int   	  headerLen;       // tamanho do header
	unsigned char  	  maskAddrOffset;  // offset do inicio da máscara nos bytes
	unsigned char  	  appDataLenBytes; // contém a quantidade de bytes necessários que guardará o tamanho do dado da aplicação

	void extractOpcode(vector<unsigned char>& buf) {
		this->opcode = buf[0] & 0xF;
	}

	void extractMaskEnabled(vector<unsigned char>& buf) {
		this->isMask = !!(buf[1] & 0x80);
	}

	void extractPayloadType(vector<unsigned char>& buf) {
		int type = buf[1] & 0x7F;

		if(type < 126) {
			maskAddrOffset = 2;
		} else if(type == 126) {
			maskAddrOffset = 4;
		} else {
			maskAddrOffset = 10;
		}

		payloadType = type;
	}

	void extractFIN(vector<unsigned char>& buf) {
		this->FIN = (buf[0] & 0x80) >> 7;
	}

public:
	void extractFirstStage(vector<unsigned char>& buf) {
		extractOpcode(buf);
		extractMaskEnabled(buf);
		extractPayloadType(buf);
		extractFIN(buf);

		headerLen 		= 2 + isMask * 4 + (this->payloadType < 126 ? 0 : this->payloadType == 126 ? 2 : 8);
		appDataLenBytes = payloadType < 126 ? 0 : payloadType == 126 ? 2 : 8;
	}

	void extractSecondStage(vector<unsigned char>& buf) {
		// read application data length
		if(payloadType < 126) {
			this->appDataLen = buf[1] & 0x7F;
		} else if(payloadType == 126) {
			ushort byteOrder  = *(ushort*)(buf.data()+2);
			this->appDataLen  = 0xFFFF & ((byteOrder >> 8) | (byteOrder << 8));
		} else {
			unsigned long byteOrder;
			memcpy(&byteOrder, buf.data()+2, this->appDataLenBytes);
			// big endian to little endian
			for(int i = 0; i < 8; i++) {
				this->appDataLen |= (0xFFFFFFFFFFFFFFFF >> 8*i) & (byteOrder >> (8*i) << (8*(7-i)));
			}
		}

		memcpy(&this->mask, &buf[maskAddrOffset], 4);
	}

	int getPayloadType() {
		return this->payloadType;
	}
};

void readNBytes(int fd, vector<unsigned char>& buf, int until) {
	buf.assign(until, 0);

	int readBytes = 0;

	while(readBytes < until) {
		readBytes += recv(fd, buf.data()+readBytes, until-readBytes, 0);
	}
}

void printHeader(PartialHeader h) {
	printf("opcode: %d\n",        h.opcode);
	printf("headerLen: %d\n",     h.headerLen);
	printf("appDataLen: %lu \n",  h.appDataLen);
	printf("FIN: %d \n",          h.FIN);
}

// buf precisa ser do tamanho exato do pacote
pair<vector<unsigned char>, bool> parsePacket(int fd, vector<unsigned char>& buf) {
	PartialHeader header;

	int bufLen = buf.size();

	// tamanho minimo de um pacote no protocolo
	if(bufLen < 2) {
		readNBytes(fd, buf, 2-bufLen);
	}

	header.extractFirstStage(buf);

	vector<unsigned char> bufTemp;

    // Tratando o caso onde recebo apenas um pedaço do header mas não ele completamente
	if(header.payloadType > 0) {
        int restBytes = bufLen-2;

		// verifico se tenho algum dado pra receber
		if(restBytes < 4) {
			int data = header.isMask * 4 + header.appDataLenBytes;

			if(header.payloadType < 126) {
				readNBytes(fd, bufTemp, data-restBytes);
			} else if(header.payloadType == 126) {
				readNBytes(fd, bufTemp, data-restBytes);
			} else {
				readNBytes(fd, bufTemp, data-restBytes);
			}

			for_each(bufTemp.begin(), bufTemp.end(), [&buf](unsigned char e) {
				buf.push_back(e);
			});
		}
	}

	header.extractSecondStage(buf);

	bufLen = buf.size();

	if(bufLen < header.appDataLen+header.headerLen) {
		readNBytes(fd, bufTemp, (header.appDataLen+header.headerLen)-bufLen);

		for_each(bufTemp.begin(), bufTemp.end(), [&buf](unsigned char e) {
			buf.push_back(e);
		});
	}

	buf.erase(buf.begin(), buf.begin()+header.headerLen+header.appDataLen);

	vector<unsigned char> data(header.appDataLen);

	for(int i = 0; i < data.size(); i++) {
		data[i] = buf[header.headerLen+i] ^ header.mask[i%4];
	}

	return { data, header.FIN };
}

void sendAll(int fd, unsigned char* buf, size_t n) {
	unsigned int sent = 0;
	while(n-sent > 0) {
		sent += send(fd, buf+sent, n-sent, 0);
	}
}

vector<unsigned char> assemblyFrames(int fd, vector<unsigned char>& buf) {
	vector<unsigned char> result;

	pair<vector<unsigned char>, bool> res;

	do {
		res      = parsePacket(fd, buf);
		auto aux = res.first;

		for_each(aux.begin(), aux.end(), [&result](unsigned char e) {
			result.push_back(e);
		});
		
	} while(!res.second);

	return result;
}

void clientHandler(int fd) {
    std::cout << "cliente conectado" << std::endl;

	unsigned char rawBuf[8192];
	int len = recv(fd, rawBuf, 8192, 0);

	string response = handshake_handler((char*)rawBuf).getResponseHeader();

	sendAll(fd, (unsigned char*)response.data(), response.size());

	vector<unsigned char> buf;

    for(;;) {
        len = recv(fd, rawBuf, 8192, 0);
		
		for_each(begin(rawBuf), begin(rawBuf)+len, [&buf](unsigned char e) {
			buf.push_back(e);
		});

		if(len) {
			printf("LEN : %d\n\n", len);
			auto data = assemblyFrames(fd, buf);

			unsigned long res = 0;
			for(auto e : data) {
				res += e;
			}

			cout << "res: " << res << endl;
		}
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

        std::thread tclient(clientHandler, client_fd);
        tclient.detach();
    }

	return 0;
}
