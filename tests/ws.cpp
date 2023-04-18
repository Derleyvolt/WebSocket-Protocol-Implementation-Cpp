#include <iostream>
#include <vector>
#include <functional>
#include <algorithm>
#include <memory>
#include <map>
#include <cstring>
#include <thread>
#include "IO.hpp"
#include "SHA1.h"
#include "base64.hpp"
#include "eventDispatcher.hpp"
#include "bitPacking.cpp"
#include "streams.cpp"
#include "../debug.cpp"

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
            // std::cout << std::endl << std::endl;
            // std::cout << response << std::endl;
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

        std::vector<uint8_t> getBinaryData() const {
            return data;
        }

        std::string getTextData() const {
            return std::string(this->data.begin(), this->data.end());
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
        int32_t           fd;
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

        std::pair<ReceiverInfoMessageFragment, bool> getFrame(PacketDataHandler& buf);
    public:
        ReceiverInfoMessageFragment message(PacketDataHandler& buf);

        ReceivePacketHandler(int32_t fileDescriptor) : fd(fileDescriptor) {
            this->appDataLen = 0;
            memset(this->mask, 0, sizeof(this->mask));
        }
    };

    class SenderPacketHandler {
    private:
        int32_t fd;
		
        // store data
        OutputMemoryStream buffer;

        // encodes FIN, RSV1, RSV2, RSV3, Opcode
        uint8_t  controlBits;

        // encodes MASK and Payload len
        uint8_t  payloadType;
        uint16_t extendedPayloadLen;
        uint64_t extendedPayloadLenContinued;
        uint8_t  mask[4];
        bool     firstFragmentSent;

        const int32_t MTU = 1400;

        void send(uint8_t* data, int32_t len, uint8_t messageType, bool FIN, bool mask) {
            buffer.reset();
            this->extendedPayloadLen = 0;
            this->extendedPayloadLenContinued = 0;
            controlBits = FIN<<7;
            controlBits = controlBits | (messageType>>(8*firstFragmentSent));
            payloadType = len <= 125 ? len : len <= 65535 ? 126 : 127;

            uint8_t highByte = (uint8_t(mask)<<7) | this->payloadType;

            buffer.write(&this->controlBits, 1);
            buffer.write(&highByte, 1);
            
            if(this->payloadType == 126) {
                this->extendedPayloadLen |= (uint16_t)len;

                // cout << "tamanho: " << this->extendedPayloadLen << endl;
                // network byte order
                if(isLittleEndian()) {
                    this->extendedPayloadLen = byteSwap2(this->extendedPayloadLen);
                }
                buffer.write(&this->extendedPayloadLen, 2);
            } else if(this->payloadType == 127) {
                this->extendedPayloadLenContinued |= (uint64_t)len;
                // network byte order
                if(isLittleEndian()) {
                    this->extendedPayloadLenContinued = byteSwap8(this->extendedPayloadLenContinued);
                }
                buffer.write(&this->extendedPayloadLenContinued, 8);
            }

            if(mask) {
                for(int i = 0; i < 4; i++) {
                    this->mask[i] = rand() % 256;
                }

                for(int i = 0; i < len; i++) {
                    data[i] = data[i] ^ this->mask[i % 4];
                }

                buffer.write(this->mask, 4);
            }

            buffer.write(data, len);

            // for(int i = 0; i < buffer.getLength(); i++) {
            //     cout << (int)buffer.getBufferPtr()[i] << " ";
            // }

            // cout << endl;

            sendAll(this->fd, (int8_t*)buffer.getBufferPtr(), buffer.getLength());
        }

    public:
        SenderPacketHandler(int32_t fileDescriptor) : controlBits(0), extendedPayloadLen(0), extendedPayloadLenContinued(0) {
            memset(mask, 0, sizeof(mask));
            this->fd = fileDescriptor;
        }

        void message(std::vector<uint8_t>& buf, uint8_t messageType = BINARY_FRAME, bool mask = false) {
            int32_t len = buf.size();
            this->firstFragmentSent = false;
            for(int i = 0; i < buf.size(); i += MTU) {
                this->send(buf.data()+i, min(MTU, len-i), messageType, MTU >= len-i, mask);
                this->firstFragmentSent = true;
            }
        }

        void ping(std::vector<uint8_t>& buf, bool mask = false) {
            message(buf, PING, mask);
        }

        void pong(std::vector<uint8_t>& buf, bool mask = false) {
            message(buf, PONG, mask);
        }

        void closeConnection(std::vector<uint8_t>& buf, bool mask = false) {
            message(buf, CLOSE_CONNECTION, mask);
        }
    };

    // buf precisa ser do tamanho exato do pacote
    // ler um frame inteiro
    std::pair<ReceiverInfoMessageFragment, bool> ReceivePacketHandler::getFrame(PacketDataHandler& buf) {
        // tamanho minimo de um pacote no protocolo
        readAtLeast(this->fd, buf, 2);

        this->extractFirstStage(buf());

        // read rest of header, if still don't read
        readUntil(this->fd, buf, this->headerLen);

        this->extractSecondStage(buf());

        // if buf don't contains entire packet, then wait until
        // buf be filled
        readUntil(this->fd, buf, this->headerLen + this->appDataLen);

        std::vector<uint8_t> data(this->appDataLen);

        // unmasking data
        for(int i = 0; i < data.size(); i++) {
            data[i] = buf[this->headerLen+i] ^ this->mask[i%4];
        }

        buf.clearPrefix(this->headerLen+this->appDataLen);

        return { ReceiverInfoMessageFragment(this->opcode, data), this->FIN };
    }

    // A saída contém a mensagem completa, e o tipo da mensagem
    ReceiverInfoMessageFragment ReceivePacketHandler::message(PacketDataHandler& buf) {
        ReceiverInfoMessageFragment appData;

        std::pair<ReceiverInfoMessageFragment, bool> out;

        do {
            out      = this->getFrame(buf);
            appData += out.first;
        } while(!out.second);

        return appData;
    }

    class TextMessageEvent : public Event {
    public:
        TextMessageEvent(std::string eventType, std::string text, SenderPacketHandler& sender) : text(text), sender(sender) {
            this->eventType = eventType;
        }

        std::string getText() const {
            return this->text;
        }


        SenderPacketHandler& sender;
    private:
        std::string text;
    };

    class BinaryMessageEvent : public Event {
    public:
        BinaryMessageEvent(std::string eventType, std::vector<uint8_t> data, SenderPacketHandler& sender) : data(data) , sender(sender) {
            this->eventType = eventType;
        }

        std::vector<uint8_t> getData() const {
            return this->data;
        }

        SenderPacketHandler& sender;
    private:
        std::vector<uint8_t> data;
    };

    class ws {
    public:
        ws(int32_t fileDescriptor) : fd(fileDescriptor), isRunning(true), sender(fileDescriptor) {
            receivePacket(fileDescriptor, this->buf, 1024);
            HandshakeHandler hs(buf.toString());
            hs.sendHTTPResponse(fileDescriptor);
        }

        void on(std::string eventType, Callback callback) {
            this->emitter.addListener(eventType, callback);
        }

        void listen() {
            this->buf.clear();
            ReceivePacketHandler        receiver(fd);
            ReceiverInfoMessageFragment info;

            this->emitter.notify("Connection");

            while(isRunning) {
                info = receiver.message(buf);

                switch(info.getMessageType()) {
                    case TEXT_FRAME: {
                        std::shared_ptr<Event> e(new TextMessageEvent("Text", info.getTextData(), sender));
                        this->emitter.notify(e.get());
                        break;
                    }

                    case BINARY_FRAME: {
                        std::shared_ptr<Event> e(new BinaryMessageEvent("Binary", {1, 2, 3, 4, 5}, sender));
                        this->emitter.notify(e.get());
                        break;
                    }

                    case CLOSE_CONNECTION: {
                        
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
        SenderPacketHandler sender;
    };
}
