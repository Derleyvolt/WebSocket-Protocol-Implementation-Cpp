#include <iostream>
#include <vector>
#include <functional>
#include <algorithm>
#include <memory>
#include "eventDispatcher.h"

namespace ws {

    void readUntil(int fd, std::vector<uint8_t>& buf, int until) {
        buf.assign(until, 0);

        int readBytes = 0;

        // while(readBytes < until) {
        // 	readBytes += recv(fd, buf.data()+readBytes, until-readBytes, 0);
        // }
    }

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
        }
    };

    class ReceiverHeaderWebSocket {
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
        std::pair<ReceiverInfoMessageFragment, bool> getFrame(int fd, std::vector<uint8_t>& buf);

        ReceiverHeaderWebSocket() {
            this->appDataLen = 0;
        }
    };

    class SenderHeaderWebSocket {
    private:
        // encodes FIN, RSV1, RSV2, RSV3, Opcode
        uint8_t  controlBits;

        // encodes MASK and Payload len
        uint8_t  payloadLen;
        uint16_t extendedPayloadLen;
        uint64_t extendedPayloadLenContinued;
        uint8_t  mask[4];
        
        

    public:


    }

    // buf precisa ser do tamanho exato do pacote
    // ler um frame inteiro
    std::pair<ReceiverInfoMessageFragment, bool> ReceiverHeaderWebSocket::getFrame(int fd, std::vector<uint8_t>& buf) {
        int bufLen = buf.size();

        // tamanho minimo de um pacote no protocolo
        if(bufLen < 2) {
            readUntil(fd, buf, 2-bufLen);
        }

        this->extractFirstStage(buf);

        std::vector<uint8_t> bufTemp;

        // Tratando o caso onde recebo apenas um pedaço do this->mas não ele completamente
        if(this->payloadType > 0) {
            int restBytes = bufLen-2;

            // verifico se tenho algum dado pra receber
            if(restBytes < 4) {
                int data = this->isMask * 4 + this->appDataLenBytes;

                if(this->payloadType < 126) {
                    readUntil(fd, bufTemp, data-restBytes);
                } else if(this->payloadType == 126) {
                    readUntil(fd, bufTemp, data-restBytes);
                } else {
                    readUntil(fd, bufTemp, data-restBytes);
                }

                for_each(bufTemp.begin(), bufTemp.end(), [&buf](uint8_t e) {
                    buf.push_back(e);
                });
            }
        }

        this->extractSecondStage(buf);

        bufLen = buf.size();

        if(bufLen < this->appDataLen+this->headerLen) {
            readUntil(fd, bufTemp, (this->appDataLen+this->headerLen)-bufLen);

            for_each(bufTemp.begin(), bufTemp.end(), [&buf](uint8_t e) {
                buf.push_back(e);
            });
        }

        buf.erase(buf.begin(), buf.begin()+this->headerLen+this->appDataLen);

        std::vector<uint8_t> data(this->appDataLen);

        ReceiverInfoMessageFragment ret;

        for(int i = 0; i < data.size(); i++) {
            data[i] = buf[this->headerLen+i] ^ this->mask[i%4];
        }

        return { ReceiverInfoMessageFragment(this->opcode, data), this->FIN };
    }

    // A saída contém a mensagem completa, e o tipo da mensagem
    ReceiverInfoMessageFragment getEntireMessage(int fd, std::vector<uint8_t>& buf) {
        ReceiverInfoMessageFragment appData;
        ReceiverHeaderWebSocket     header;

        std::pair<ReceiverInfoMessageFragment, bool> out;

        do {
            out = header.getFrame(fd, buf);
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
        }

        void on(std::string eventType, Callback callback) {
            this->emitter.addListener(eventType, callback);
        }   

        void listen() {
            std::vector<uint8_t> buf;
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
        Dispatcher emitter;
        int32_t    fd;
        bool       isRunning;
    };
}