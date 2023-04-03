#include <iostream>
#include <vector>
#include <memory>

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

uint64_t bswap(uint64_t src, int8_t nBytes) {
    array<nBytes, uint8_t> bytes;

    for(int i = 0; i < nBytes/2; i++) {

    }

    return src;
} 

// a b c d e
// 

// 0101 shift <-> 1000 | 0101 = 11101
// 1100 shift <-> 0011 | 0000 <-> 0011
//                                1100

// 1001 1100
// 1100 1011

class Buffer {
public:
    Buffer() {

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
    uint8_t*  data;
    int32_t   size;
    uint32_t  index;
    bool      endianess;
};

int main() {
    uint16_t a = 0b1111111100000000;
    // a = bswap(a, 2);
    // printBits(a, 16);
    bswap(a, 2);
    return 0;
}
