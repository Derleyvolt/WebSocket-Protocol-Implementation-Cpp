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

void showBits(unsigned short n) {
	if(n <= 0) {
		return;
	}

	showBits(n>>1);
	printf("%d", (n&1));
}

class PartialHeader {
public:
	unsigned char  opcode;
	unsigned char  isMask;
	unsigned char  payloadType;
	unsigned char  FIN;
	unsigned int   appDataLen;
	unsigned char  mask[4];
	unsigned int   headerLen;
	unsigned char  maskAddrOffset;

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
			payloadType = type;
		} else if(type == 126) {
			maskAddrOffset = 4;
			payloadType = type;
		} else {
			maskAddrOffset = 10;
			payloadType = type;
		}
	}

	void extractFIN(vector<unsigned char>& buf) {
		this->FIN = buf[0] & 0x80 >> 7;
	}

public:
	int getOffsetToSecondStage() const {	
		return this->maskAddrOffset + 2;
	}

	void extractFirstStage(vector<unsigned char>& buf) {
		extractOpcode(buf);
		extractMaskEnabled(buf);
		extractPayloadType(buf);
		extractFIN(buf);

		headerLen = 2 + isMask * 4 + (this->payloadType < 126 ? 0 : this->payloadType == 126 ? 2 : 8);
	}

	void extractSecondStage(vector<unsigned char>& buf) {
		// read application data length
		if(payloadType < 126) {
			this->appDataLen = buf[1] & 0x7F;
		} else if(payloadType == 126) {
			ushort byteOrder  = *(ushort*)(buf.data()+2);
			this->appDataLen  = 0xFFFF & ((byteOrder >> 8) | (byteOrder << 8));
		} else {
			this->appDataLen = 'TRATAR O ENDIANESS';
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
	printf("opcode: %d\n",     h.opcode);
	printf("mask: %d\n",       h.isMask);
	printf("headerLen: %d\n", h.headerLen);
	printf("appDataLen: %d\n", h.appDataLen);
	printf("FIN: %d\n\n",      h.FIN);
}

// buf precisa ser do tamanho exato do pacote
void parseHeader(int fd, vector<unsigned char>& buf) {
	PartialHeader header;

	header.extractFirstStage(buf);

	printHeader(header);

	vector<unsigned char> bufTemp;

	cout << "getOffsetToSecondStage: " << header.getOffsetToSecondStage() << endl;

	cout << "buf: " << buf.size() << endl;

    // Tratando o caso onde recebo apenas um pedaço do header mas não ele completamente
	if(buf.size() < 14) {
        int restBytes = x-header.headerLen;

        if(header.payloadType < 126 && restBytes < 4) {
            readNBytes(fd, bufTemp, 4);    
        } else if(header.payloadType == 126 && restBytes < 6) {
            readNBytes(fd, bufTemp, 6);
        } else {
            readNBytes(fd, bufTemp, 12);
        }
	}

	cout << "tamanho: " << bufTemp.size() << endl;

	for_each(bufTemp.begin(), bufTemp.end(), [&buf](unsigned char e) {
		buf.push_back(e);
	});

	header.extractSecondStage(buf);

	printHeader(header);

	buf.erase(buf.begin(), buf.begin()+header.headerLen+header.appDataLen);

	cout << buf.size() << endl;

	// h.opcode	 = buf[0] & 0xF;
	// h.isMask     = buf[1] & 0x80;
	// h.length     = buf[1] & 0x7F;
	// h.FIN 		 = buf[0] & 0x80 >> 7;
	// h.headerLen  = 2 + (!!h.isMask) * 4 + h.length == 126 ? 2 : 8;

	// if(h.length < 126) {
	// 	h.payloadLen = h.length;
	// 	h.payLoadBuffer.resize(h.length);
	// 	printf("mask\n");
	// 	printf("%d %d %d %d\n", buf[2], buf[3], buf[4], buf[5]);
	// 	memcpy(&h.mask, &buf[2], 4);
	// } else if(h.length == 126) {
	// 	ushort byteOrder = *(ushort*)(&buf[2]);
	// 	h.payloadLen     = 0xFFFF & ((byteOrder >> 8) | (byteOrder << 8));
	// 	printf("%d %d %d %d\n", buf[4], buf[5], buf[6], buf[7]);
	// 	memcpy(&h.mask, &buf[4], 4);
	// } else if(h.length == 127) {
	// 	memcpy(&h.payloadLen, buf.data()+2, 8);
	// 	memcpy(&h.mask, &buf[10], 4);
	// }
  
	// if(h.isMask) {
	// 	if(h.payloadLen < 126) {
	// 		memcpy(&h.mask, &buf[2], 4);
	// 	} else if(h.payloadLen == 126) {
	// 		memcpy(&h.mask, &buf[4], 4);
	// 	} else {
	// 		memcpy(&h.mask, &buf[10], 4);
	// 	}
	// }

	// h.payLoadBuffer.resize(h.payloadLen);
	// memcpy(h.payLoadBuffer.data(), &buf[h.length < 126 ? 6 : 8], h.payloadLen);

	// printf("buffer\n");

	// for(int i = 0; i < h.payLoadBuffer.size(); i++) {
	// 	cout << (char)(h.payLoadBuffer[i] ^ h.mask[i%4]) << " ";
	// }

	// cout << endl;
	// return h;
}

void clientHandler(int fd) {
    std::cout << "cliente conectado" << std::endl;

	unsigned char rawBuf[8192];
	int len = recv(fd, rawBuf, 8192, 0);

	string response = handshake_handler((char*)rawBuf).getResponseHeader();

	// tratar isso aqui dps.. while sendBytes > 0 ..
	send(fd, response.data(), response.size(), 0);

	vector<unsigned char> buf;

    for(;;) {
        len = recv(fd, rawBuf, 8192, 0);
		
		for_each(begin(rawBuf), begin(rawBuf)+len, [&buf](unsigned char e) {
			buf.push_back(e);
		});

		if(len) {
			printf("LEN : %d\n\n", len);
			parseHeader(fd, buf);
		}

		// unsigned char opcode     = 0;
		// unsigned char FIN        = 0;
		// unsigned char payLoadLen = 0;
		// unsigned char mask       = 0;
		// if(len) {
		// 	FIN    	   = (buf[0] & 0x80) >> 7;
		// 	opcode     = buf[0]  & 0xF;
		// 	payLoadLen = buf[1]  & 0x7F;
		// 	mask       = buf[1]  & 0x80;
		// 	printf("mask: %d\n", mask);
		// 	printf("payLoadLen: %d\n", payLoadLen);
		// 	printf("opcode: ");
		// 	showBits(opcode);
		// 	printf("\n");
		// 	printf("FIN: %d\n\n", FIN);
		// 	showBits(buf[1]);
		// 	printf("\n");
		// }
		// if(len > 0) {
		// 	for(int i = 0; i < len; i++) {
		// 		printf("%d ", buf[i]);
		// 	}
		// 	printf("\n");
		// }
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
