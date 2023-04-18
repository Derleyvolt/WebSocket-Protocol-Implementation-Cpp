#include <iostream>

using namespace std;

// void printBits(uint64_t arg, int n) {
//     if(n <= 0) {
//         return;
//     }

//     printBits(arg>>1, n-1);
//     printf("%d", (arg&1));
// }

uint16_t byteSwap2(uint16_t data) {
    return (data >> 8) | (data << 8);
}

uint32_t byteSwap4(uint32_t data) {
    return  ((data >> 24) & 0x000000ff) |
            ((data >> 8) & 0x0000ff00)  |
            ((data << 8) & 0x00ff0000)  |
            ((data << 24) & 0xff000000);
}

uint64_t byteSwap8(uint64_t data) {
    return  ((data >> 56) & 0x00000000000000ff)  |
            ((data >> 40) & 0x000000000000ff00)  |
            ((data >> 24) & 0x0000000000ff0000)  |
            ((data >> 8)  & 0x00000000ff000000)  |
            ((data << 8)  & 0x000000ff00000000)  |
            ((data << 24) & 0x0000ff0000000000)  |
            ((data << 40) & 0x00ff000000000000)  |
            ((data << 56) & 0xff00000000000000);
} 

bool isLittleEndian() {
    uint16_t word(1);
    return *(uint8_t*)&word;
}