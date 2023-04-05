#include <string>
#include <algorithm>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <memory>

class PacketDataHandler {
public:
    PacketDataHandler() {
    }

    PacketDataHandler(uint32_t size) {
        this->buf = std::vector<uint8_t>(size);
    }

    void clear() {
        this->buf.clear();
    }

    void push_back(uint8_t e) {
        this->buf.push_back(e);
    }

    void clearPrefix(uint32_t len) {
        this->buf.erase(this->buf.begin(), this->buf.begin() + len);
    }

    int32_t length() {
        return this->buf.size();
    }

    void pop_back() {
        this->buf.pop_back();
    }

    void operator+=(PacketDataHandler &rhs) {
        std::for_each(rhs.buf.begin(), rhs.buf.end(), [this](uint8_t e) { 
            this->buf.push_back(e);
        });
    }

    std::string toString() {
        return std::string(this->buf.begin(), this->buf.end());
    }

    std::vector<uint8_t> getBuffer() {
        return this->buf;
    }

    uint8_t *data() {
        return this->buf.data();
    }

    uint8_t& operator[](uint32_t idx) {
        return buf[idx];
    }

private:
    std::vector<uint8_t> buf;
};

// bCount = limite, pode ser ajustado, mas acredito que isso seja
// suficiente pro fluxo de mensagens
int32_t receivePacket(int32_t fd, PacketDataHandler &buf, int32_t bCount = 256) {
    std::unique_ptr<uint8_t> localBuf(new uint8_t[bCount]);

    int32_t count = recv(fd, localBuf.get(), bCount, 0);

    for (int i = 0; i < count; i++) {
        buf.push_back(localBuf.get()[i]);
    }

    return count;
}

// Bloqueia a thread atual até que pelo menos bCount bytes sejam lidos
void readAtLeast(int32_t fd, PacketDataHandler &buf, int32_t bCount) {
    while (bCount > 0) {
        bCount -= receivePacket(fd, buf);
    }
}

// Bloqueia a thread atual até que pelo menos bCount bytes esteja em buf
void readUntil(int32_t fd, PacketDataHandler &buf, int32_t bCount) {
    while (buf.length() < bCount) {
        receivePacket(fd, buf);
    }
}

// Bloqueia a thread atual até envia os bCount bytes ao endpoint
void sendAll(int32_t fd, int8_t *buf, int bCount) {
    int32_t sent = 0;
    while (sent < bCount) {
        sent += send(fd, buf + sent, bCount - sent, 0);
    }
}
