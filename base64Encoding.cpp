#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "SHA1.h"

using namespace std;

const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

typedef unsigned char int8;

typedef struct dynamic_arr {
    int8* buf;
    int len;
    int capacity;
} array_d;

array_d* inicialize_array() {
    array_d* arr    = (array_d*)malloc(sizeof(array_d));
    arr->len      = 0;
    arr->capacity = 100;
    arr->buf      = (int8*)malloc(sizeof(int8) * 100);
    return arr;
}

void add(array_d* arr, int8 val) {
    if(arr->capacity == arr->len) {
        int8* aux = (int8*)malloc(sizeof(int8) * arr->capacity * 2);

        for(int i = 0; i < arr->capacity; i++) {
            aux[i] = arr->buf[i];
        }

        free(arr->buf);

        arr->capacity = arr->capacity * 2;
        arr->buf = aux;
    }

    arr->buf[arr->len] = val;
    arr->len++;
}

void mapping(array_d* arr, int pad) {
    for(int i = 0; i < arr->len; i++) {
        arr->buf[i] = i+pad >= arr->len ? '=' : b64[arr->buf[i]];
    }
}

void print_arr(array_d* arr) {
    for(int i = 0; i < arr->len; i++) {
        printf("%c", arr->buf[i]);
    }
}

array_d* b64_encode(int8* buf, int len) {
    int pad = 0;

    if(len % 3 > 0) {
        pad       = (3 - len % 3);
        int8* aux = (int8*)malloc(sizeof(int8) * len + (3 - len % 3));

        for(int i = 0; i < len + (3 - len%3); i++) {
            aux[i] = i < len ? buf[i] : 0;
        }

        buf = aux;
        len = len + (3 - len%3);
    }

    array_d* arr  = inicialize_array();

    int byte;

    for(int i = 0; i < len; i+=3) {
        byte = 0;

        for(int j = 0; j < 3; j++) {
            byte = byte | (buf[i+j]<<(16-(j*8)));
        }

        for(int j = 0; j < 4; j++) {
            add(arr, (byte & (0x3F << (18-(j*6)))) >> 18-(j*6));
        }
    }
    
    mapping(arr, pad);
    return arr;
}

std::string response_ws(string base64, string sha1) {
    string sec = base64+sha1;
    SHA1 sh;
    sh.update(sec);

    int8* buf = sh.final();

    array_d* arr = b64_encode(buf, 20);

    string res;

    for(int i = 0; i < arr->len; i++) {
        res.push_back(arr->buf[i]);
    }

    return res;
}

// int mainy() {
//     string key = "p8+G982PQlgt6k9cJkRr5Q==";
//     string sha = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
//     // SHA1 sh;

//     // sh.update(A);

//     // int8* buf = sh.final();

//     // array_d* arr = b64_encode(buf, 20);
//     // print_arr(arr);
    
//     cout << endl;
//     cout << response_ws(key, sha) << endl;
//     return 0;
// }
