#include <iostream>
#include <vector>
#include <map>
#include <string.h>
#include <algorithm>
#include <stack>

using namespace std;

struct Node;

struct List {
    vector<Node*> list;
};

struct Object {
    map<string, Node*> t;
};

struct Node {
    enum Type { OBJECT, JSONLIST, STR, BOOLEAN, REAL };

    union {
        Object*    json;
        List*      jsonList;
        string*    str;
        bool       boolean;
        float      real;
    } data;

    Type type;
};

// 3F3F3F3F

map<string, Node*> JSON;

void printBits(uint32_t n, int countBits) {
    if(countBits <= 0) {
        return;
    }

    printBits(n>>1, countBits-1);
    printf("%d", (n&1));
}

enum ValueType {
    OBJECT,
    LIST,
    STRING,
    LITERAL
};

// espera apenas espaços
string getKeyFromJSON(string& JSON) {

}

// espera apenas espaços ou vírgulas precedendo o value
string getValueFromJSON(string& JSON) {
    stack<char> aux;
    string value;

    // skip vírgula e blank spaces
    for(int i = 0; i < JSON.size(); i++) {
        if(JSON[i] != ',' && JSON[i] != ' ') {
            JSON = JSON.substr(i);
            break;
        }
    }

    char search = JSON.front();
    value.push_back(search);

    map<char, char> mapper;

    mapper['['] = ']';
    mapper['{'] = '}';
    mapper['"'] = '"';

    // Essa branch trata os casos onde temos uma lista, um objeto ou uma string

    // A ideia é só generalizar o problema do 'valid parentheses problem'.
    if(search == '[' || search == '{' || search == '"') {
        aux.push(search);

        for(int i = 1; i < JSON.size(); i++) {
            char e = JSON[i];
            // se eu encontro um simbolo que case com o topo da stack
            // eu tiro o par da stack (sempre terá um par na stack visto que o JSON recebido é sempre válido [O front-end me garante que é válido]).
            if(mapper[search] == e) {
                aux.pop();
                // se a stack estiver vazio significa que chegamos no fim da lista/objeto/string
                if(aux.empty()) {
                    value.push_back(e);
                    JSON = JSON.substr(i+1);
                    return value;
                }
            }

            value.push_back(e);

            if(e == search) {
                aux.push(search);
            }
        }
    } else {
        int i;
        // Nessa branch temos um Number ou um Literal
        for(i = 1; i < JSON.size(); i++) {
            char e = JSON[i];
            if(e != '[' && e != '{' && e != ',') {
                value.push_back(e);
            } else {
                JSON = JSON.substr(i+!!(e == ','));
                return value;
            }
        }

        // se chego aqui significa que a string acabou
        JSON.clear();
        return value;
    }
}

// array<string, 2> split(string& JSON, int index) {
//     string key, value;
//     key   = JSON.substr(0, index);
//     value = JSON.substr(index+1, JSON.);
// }   

// map<string, JSON*> parserKeyValue(string& JSON) {
//     map<string, string> aux;

//     JSON = string(JSON.begin()+1, JSON.end()-1);

//     // while(1) {
//     //     string key = JSON.substr(0, JSON.find())
//     // }
// }

int main() {
    string s = "\"Age\",{\"Lista\":[1,2,3,4,5]},1,2,3,\"rato\"";

    while(s.size() > 0) {
        cout << getValueFromJSON(s) << endl;
    }

}

// {
//     "Name": "Jonh",
//     "Age": 34,
//     "StateOfOrigin": "England",
//     "Pets": [
//         {
//             "Type": "Cat",
//             "Name": "MooMoo",
//             "Age": 3.4
//         }, 
//         {
//             "Type": "Squirrel",
//             "Name": "Sandy",
//             "Age": 7
//         }
//     ]
// }
