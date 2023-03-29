#include <iostream>
#include <vector>
#include <functional>
#include <algorithm>

void readUntil(int fd, std::vector<uint8_t>& buf, int until) {
	buf.assign(until, 0);

	int readBytes = 0;

	while(readBytes < until) {
		readBytes += recv(fd, buf.data()+readBytes, until-readBytes, 0);
	}
}

#define TEXT_FRAME        0x1
#define BINARY_FRAME      0x2
#define CLOSE_CONNECTION  0x8
#define PING              0x9
#define PONG              0xA

class InfoMessageFragment {
private:
    uint8_t               messageType;
    std::vector<uint8_t>  data;
public:
    InfoMessageFragment() : messageType(0) {
    }

    InfoMessageFragment(uint8_t messageType, std::vector<uint8_t> data) : messageType(messageType), data(data) {
    }

    uint8_t getMessageType() const {
        return messageType;
    }

    std::vector<uint8_t> getData() const {
        return data;
    }

    InfoMessageFragment operator+=(InfoMessageFragment& rhs) {
        for_each(rhs.data.begin(), rhs.data.end(), [&](uint8_t e) {
            data.push_back(e);
        });

        this->messageType = std::max(rhs.messageType, this->messageType);
    }
};

class HeaderWebSocket {
public:
	uint8_t           opcode;          // opcode..
	bool  		      isMask;          // máscara
	uint8_t           payloadType;     // tipo referente ao tamanho do dado da aplicação
	uint8_t           FIN;             // usado pra verificar se existem múltiplos frames referente a um mesmo dado
	unsigned long int appDataLen;      // tamanho do dado da aplicação
	uint8_t  	      mask[4];         // máscara
	unsigned int   	  headerLen;       // tamanho do header
	uint8_t  	      maskAddrOffset;  // offset do inicio da máscara nos bytes
	uint8_t  	      appDataLenBytes; // contém a quantidade de bytes necessários que guardará o tamanho do dado da aplicação

    HeaderWebSocket() {
        this->appDataLen = 0;
    }

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

public:
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

	int getPayloadType() {
		return this->payloadType;
	}
};

// buf precisa ser do tamanho exato do pacote
// ler um frame inteiro
std::pair<InfoMessageFragment, bool> getFrame(int fd, std::vector<uint8_t>& buf) {
	HeaderWebSocket header;

	int bufLen = buf.size();

	// tamanho minimo de um pacote no protocolo
	if(bufLen < 2) {
		readUntil(fd, buf, 2-bufLen);
	}

	header.extractFirstStage(buf);

	std::vector<uint8_t> bufTemp;

    // Tratando o caso onde recebo apenas um pedaço do header mas não ele completamente
	if(header.payloadType > 0) {
        int restBytes = bufLen-2;

		// verifico se tenho algum dado pra receber
		if(restBytes < 4) {
			int data = header.isMask * 4 + header.appDataLenBytes;

			if(header.payloadType < 126) {
				readUntil(fd, bufTemp, data-restBytes);
			} else if(header.payloadType == 126) {
				readUntil(fd, bufTemp, data-restBytes);
			} else {
				readUntil(fd, bufTemp, data-restBytes);
			}

			for_each(bufTemp.begin(), bufTemp.end(), [&buf](uint8_t e) {
				buf.push_back(e);
			});
		}
	}

	header.extractSecondStage(buf);

	bufLen = buf.size();

	if(bufLen < header.appDataLen+header.headerLen) {
		readUntil(fd, bufTemp, (header.appDataLen+header.headerLen)-bufLen);

		for_each(bufTemp.begin(), bufTemp.end(), [&buf](uint8_t e) {
			buf.push_back(e);
		});
	}

	buf.erase(buf.begin(), buf.begin()+header.headerLen+header.appDataLen);

	std::vector<uint8_t> data(header.appDataLen);

    InfoMessageFragment ret;

	for(int i = 0; i < data.size(); i++) {
		data[i] = buf[header.headerLen+i] ^ header.mask[i%4];
	}

    return { InfoMessageFragment(header.opcode, data), header.FIN };
}

// A saída contém a mensagem completa, e o tipo da mensagem
InfoMessageFragment getEntireMessage(int fd, std::vector<uint8_t>& buf) {
    InfoMessageFragment appData;

    std::pair<InfoMessageFragment, bool> out;

    do {
        out = getFrame(fd, buf);
        appData += out.first;
    } while(!out.second);

	return appData;
}

class ws {
public:
    ws(int32_t fileDescriptor) : fd(fileDescriptor), isRunning(true) {
    }

    void onConnection(std::function<void()> f) {
        this->connectionCallback = f;
    }   

    void onMessage(std::function<void(uint8_t)> f) {
        this->messageCallback = f;
    }

    void onClose(std::function<void()> f) {
        this->closeCallback = f;
    }

    void listen() {
        std::vector<uint8_t> buf;
        InfoMessageFragment info;

        connectionCallback();

        while(isRunning) {
            info = getEntireMessage(this->fd, buf);

            if(info.getMessageType() == TEXT_FRAME || info.getMessageType() == BINARY_FRAME) {


            } else if(info.getMessageType() == CLOSE_CONNECTION) {
                this->isRunning = false;
                this->closeCallback();
            } else if(info.getMessageType() == PING) {

            } else if(info.getMessageType() == PONG) {

            }
        }
    }

private:
    std::function<void()>        connectionCallback;
    std::function<void(uint8_t)> messageCallback;
    std::function<void()>        closeCallback;
    int32_t fd;
    bool    isRunning;
};
