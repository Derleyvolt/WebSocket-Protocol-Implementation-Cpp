#include <iostream>
#include <vector>
#include <memory>
#include <bitset>

using namespace std;

void printBits(uint64_t arg, int n) {
    if(n <= 0) {
        return;
    }

    printBits(arg>>1, n-1);
    printf("%d", (arg&1));
}

void setBit(uint64_t& src, int8_t pos) {
    src = src | (1LL << pos);
}

void clearBit(uint64_t& src, int32_t pos) {
    src = src & ~(1LL << pos);
}

void swap(uint8_t& a, uint8_t& b) {
    uint8_t aux = a;
    a = b;
    b = aux;
}

uint16_t bswap(uint16_t src) {
    uint8_t* bytes = (uint8_t*)&src;
    int32_t n      = sizeof(uint16_t);

    for(int i = 0; i < n/2; i++) {
        swap(bytes[i], bytes[n-1-i]);
    }

    return *(uint16_t*)bytes;
}

uint32_t bswap(uint32_t src) {
    uint8_t* bytes = (uint8_t*)&src;
    int32_t n      = sizeof(uint32_t);

    for(int i = 0; i < n/2; i++) {
        swap(bytes[i], bytes[n-1-i]);
    }

    return *(uint32_t*)bytes;
}

uint64_t bswap(uint64_t src) {
    uint8_t* bytes = (uint8_t*)&src;
    int32_t n      = sizeof(uint64_t);

    for(int i = 0; i < n/2; i++) {
        swap(bytes[i], bytes[n-1-i]);
    }

    return *(uint64_t*)bytes;
} 

class Buffer {
public:
    Buffer(int32_t size) : size(size), index(0) {
        data = new uint8_t[this->size];
    }

    void writeUint8(uint8_t arg) {

    }

    void writeUint16(uint16_t arg) {

    }

    void writeUInt32(uint32_t arg) {

    }

    void writeUint64(uint64_t arg) {

    }

    uint8_t readUint8() {
        return 1;
    }

    uint16_t readUint16() {
        return 1;
    }

    uint32_t readUint32() {
        return 1;
    }

    uint64_t readUInt64() {
        return 1;
    }

private:
    bool endianess() {
        this->isMachineLittleEndian = 1&1 == 1 ? 1 : 0;
    }

    uint8_t*  data;
    int32_t   size;
    uint32_t  index;
    bool      isMachineLittleEndian;
};

int main() {
    uint16_t x = 1;
    // x = bswap(x);
    printBits(x, 16);
    cout << endl;
    int p = (*(uint8_t*)&x);
    cout << p << endl;
    // cout << (bswap(uint32_t(1))&1) << endl;
    return 0;
}
