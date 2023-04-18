#include <vector>
#include <iostream>
#include <string.h>

using namespace std;

class OutputMemoryStream {
public:
    OutputMemoryStream(): buffer(nullptr), length(0), capacity(0) {
        reallocBuffer(64);
    }

    ~OutputMemoryStream() {
        delete[] buffer;
    }

    //get a pointer to the data in the stream
    const uint8_t* getBufferPtr() const {
        return buffer;
    }

    uint32_t getLength() const {
        return length;
    }

    void write(const void* src, size_t byteCount);

    void write(uint32_t inData) {
        write(&inData, sizeof(inData));
    }

    void write(int32_t inData) {
        write(&inData, sizeof(inData));
    }

    void reset() {
        this->length = 0;
    }
private:
    void      reallocBuffer(uint32_t newLength);
    uint8_t*  buffer;
    uint32_t  length;
    uint32_t  capacity;
};

void OutputMemoryStream::reallocBuffer(uint32_t newLength) {
    uint8_t* aux = new uint8_t[newLength];

    for(int i = 0; i < this->length; i++) {
        aux[i] = this->buffer[i];
    }

    delete[] this->buffer;

    this->buffer   = aux;
    this->capacity = newLength;
    this->length   = length;
}

void OutputMemoryStream::write(const void* src, size_t byteCount) {
    if(byteCount+this->length > this->capacity) {
        this->reallocBuffer((byteCount+this->length) * 2);
    }

    memcpy(this->buffer+this->length, src, byteCount);
    this->length += byteCount;
}

class InputMemoryStream {
public:
    InputMemoryStream(char* buffer, uint8_t byteCount) : capacity(byteCount), length(0)  {
    }

    ~InputMemoryStream() {
        delete[] this->buffer;
    }

    void read(void* dest, uint32_t byteCount) {
        memcpy(dest, this->buffer, byteCount);
        this->length -= byteCount;
    }

private:
    uint32_t capacity;
    uint32_t length;
    uint8_t* buffer;
};

void printBits(uint32_t val, int bCount) {
    if(bCount <= 0) {
        cout << endl;
        return;
    }

    printBits(val>>1, bCount-1);
    cout << (val&1);
}