#include <iostream>
#include <vector>
#include <functional>
#include <algorithm>
#include <memory>
#include <map>
#include <cstring>
#include <thread>
#include "eventDispatcher.hpp"
#include "IO.hpp"
#include "SHA1.h"
#include "base64.hpp"

namespace ws {
    class HandshakeHandler {
    public:
        HandshakeHandler(std::string headers) {
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

        void sendHTTPResponse(int32_t fp) {
            std::string response = this->getHTTPResponse();
            sendAll(fp, (int8_t*)response.data(), response.size());
        }

        std::string getHeader(std::string h) {
            return header[h];
        }

    private:
        std::map<std::string, std::string> header;

        std::string getSecurityCheck() {
            return genAcceptHeader(this->header["Sec-WebSocket-Key"], "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        }

        std::string genAcceptHeader(std::string base64, std::string GUID) {
            SHA1 sh;
            sh.update(base64+GUID);
            uint8_t* buf = sh.final();
            std::string digest;
            for(int i = 0; i < 20; i++) {
                digest.push_back(buf[i]);
            }

            return base64::to_base64(digest);
        }

        std::string getHTTPResponse() {
            std::string res_header = "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + this->getSecurityCheck() + "\r\n\r\n";
            return res_header;
        }
    };

    #define TEXT_FRAME        0x1
    #define BINARY_FRAME      0x2
    #define CLOSE_CONNECTION  0x8
    #define PING              0x9
    #define PONG              0xA

    class ReceiverInfoMessageFragment {
    private:
        uint8_t               messageType;
        std::vector<uint8_t>  data;
    public:
        ReceiverInfoMessageFragment() : messageType(0) {
        }

        ReceiverInfoMessageFragment(uint8_t messageType, std::vector<uint8_t> data) : messageType(messageType), data(data) {
        }

        uint8_t getMessageType() const {
            return messageType;
        }

        std::vector<uint8_t> getData() const {
            return data;
        }

        ReceiverInfoMessageFragment operator+=(ReceiverInfoMessageFragment& rhs) {
            for_each(rhs.data.begin(), rhs.data.end(), [&](uint8_t e) {
                data.push_back(e);
            });

            this->messageType = std::max(rhs.messageType, this->messageType);

            return *this;
        }
    };

    class ReceivePacketHandler {
    private:
        uint8_t           opcode;          // opcode..
        bool  		      isMask;          // máscara
        uint8_t           payloadType;     // tipo referente ao tamanho do dado da aplicação
        uint8_t           FIN;             // usado pra verificar se existem múltiplos frames referente a um mesmo dado
        uint64_t          appDataLen;      // tamanho do dado da aplicação
        uint8_t  	      mask[4];         // máscara
        uint32_t   	      headerLen;       // tamanho do header
        uint8_t  	      maskAddrOffset;  // offset do inicio da máscara nos bytes
        uint8_t  	      appDataLenBytes; // contém a quantidade de bytes necessários que guardará o tamanho do dado da aplicação

        void extractOpcode(std::vector<uint8_t>& buf) {
            this->opcode = buf[0] & 0xF;
        }

        void extractMaskEnabled(std::vector<uint8_t>& buf) {
            this->isMask = !!(buf[1] & 0x80);
        }

        void extractPayloadType(std::vector<uint8_t>& buf) {
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

        void extractFIN(std::vector<uint8_t>& buf) {
            this->FIN = (buf[0] & 0x80) >> 7;
        }

        void extractFirstStage(std::vector<uint8_t>& buf) {
            extractOpcode(buf);
            extractMaskEnabled(buf);
            extractPayloadType(buf);
            extractFIN(buf);

            headerLen 		= 2 + isMask * 4 + (this->payloadType < 126 ? 0 : this->payloadType == 126 ? 2 : 8);
            appDataLenBytes = payloadType < 126 ? 0 : payloadType == 126 ? 2 : 8;
        }

        void extractSecondStage(std::vector<uint8_t>& buf) {
            // read application data length
            if(payloadType < 126) {
                this->appDataLen = buf[1] & 0x7F;
            } else if(payloadType == 126) {
                uint16_t byteOrder  = *(uint16_t*)(buf.data()+2);
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

    public:
        std::pair<ReceiverInfoMessageFragment, bool> getFrame(int fd, PacketDataHandler& buf);

        ReceivePacketHandler() {
            this->appDataLen = 0;
            memset(this->mask, 0, sizeof(this->mask));
        }
    };

    class SenderPacketHandler {
    private:
        // encodes FIN, RSV1, RSV2, RSV3, Opcode
        uint8_t  controlBits;

        // encodes MASK and Payload len
        uint8_t  payloadLen;
        uint16_t extendedPayloadLen;
        uint64_t extendedPayloadLenContinued;
        uint8_t  mask[4];
        
        

    public:

    };

    // buf precisa ser do tamanho exato do pacote
    // ler um frame inteiro
    std::pair<ReceiverInfoMessageFragment, bool> ReceivePacketHandler::getFrame(int fd, PacketDataHandler& buf) {
        //int bufLen = buf.size();
        
        // tamanho minimo de um pacote no protocolo
        readAtLeast(fd, buf, 2);

        this->extractFirstStage(buf());

        // Tratando o caso onde recebo apenas um pedaço do pacote mas não ele completamente
        // 
        if(this->payloadType > 0) {
            int32_t packetLen = this->isMask * 4 + this->appDataLenBytes + this->appDataLen;

            // if buf don't contains entire packet, then wait until
            // buf be filled
            readUntil(fd, buf, packetLen);
        }

        this->extractSecondStage(buf());

        std::vector<uint8_t> data(this->appDataLen);

        // unmasking data
        for(int i = 0; i < data.size(); i++) {
            data[i] = buf[this->headerLen+i] ^ this->mask[i%4];
        }

        buf.clearPrefix(this->headerLen+this->appDataLen);

        return { ReceiverInfoMessageFragment(this->opcode, data), this->FIN };
    }

    // A saída contém a mensagem completa, e o tipo da mensagem
    ReceiverInfoMessageFragment getEntireMessage(int fd, PacketDataHandler& buf) {
        ReceiverInfoMessageFragment appData;
        ReceivePacketHandler        header;

        std::pair<ReceiverInfoMessageFragment, bool> out;

        do {
            out      = header.getFrame(fd, buf);
            appData += out.first;
        } while(!out.second);

        return appData;
    }

    class TextMessageEvent : public Event {
    public:
        TextMessageEvent(std::string eventType, std::string text) : text(text) {
            this->eventType = eventType;
        }

        std::string getText() const {
            return this->text;
        }
    private:
        std::string text;
    };

    class BinaryMessageEvent : public Event {
    public:
        BinaryMessageEvent(std::string eventType, std::vector<uint8_t> data) : data(data) {
            this->eventType = eventType;
        }

        std::vector<uint8_t> getData() const {
            return this->data;
        }
    private:
        std::vector<uint8_t> data;
    };

    class ws {
    public:
        ws(int32_t fileDescriptor) : fd(fileDescriptor), isRunning(true) {
            receivePacket(fileDescriptor, this->buf);
            HandshakeHandler hs(buf.toString());
            hs.sendHTTPResponse(fileDescriptor);
        }

        void on(std::string eventType, Callback callback) {
            this->emitter.addListener(eventType, callback);
        }   

        void listen() {
            this->buf.clear();
            ReceiverInfoMessageFragment info;

            this->emitter.notify("Connection");

            while(isRunning) {
                info = getEntireMessage(this->fd, buf);

                switch(info.getMessageType()) {
                    case TEXT_FRAME: {
                        std::shared_ptr<Event> e(new TextMessageEvent("textMessage", "something"));
                        this->emitter.notify(e.get());
                        break;
                    }

                    case BINARY_FRAME: {
                        std::shared_ptr<Event> e(new BinaryMessageEvent("binaryMessage", {1, 2, 3, 4, 5}));
                        this->emitter.notify(e.get());
                        break;
                    }

                    case CLOSE_CONNECTION: {
                        // UDP e TCP
                        break;
                    }

                    case PING: {

                        break;
                    }

                    case PONG: {

                        break;
                    }
                }
            }
        }

    private:
        Dispatcher        emitter;
        int32_t           fd;
        bool              isRunning;
        PacketDataHandler buf;
    };
}

#define PORT 8023

void clientHandler(int fd) {

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
