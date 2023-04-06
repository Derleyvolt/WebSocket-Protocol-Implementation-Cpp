#include "ws.cpp"

#define PORT 8023

void clientHandler(int32_t fd) {
    ws::ws conn(fd);

    conn.on("Text", [](Event* e) {
        ws::TextMessageEvent* ev = (ws::TextMessageEvent*)e;
        std::cout << ev->getText() << std::endl;
    });

    conn.listen();
}

int main(int argc, char const* argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt           = 1;
    int addrlen       = sizeof(address);

    {
        // Creating socket file descriptor
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        // Forcefully attaching socket to the port 8080
        // É opcional, no entanto ajuda a reusar uma porta.. evita erros do tipo: 'a porta já está em uso'
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        address.sin_family       = AF_INET;     // ipv4
        address.sin_addr.s_addr  = INADDR_ANY;  // ip local
        address.sin_port         = htons(PORT); // passando os bytes da porta pra 'network byte order'

        // Forcefully attaching socket to the port 8080
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, 20) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        std::cout << "Server iniciado" << std::endl;
    }
    
    // ACEITA REQUISIÇÕES DE CLIENTES
    for(;;) {
        // blocking
        if ((client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            std::cout << "Erro na funcao accept" << std::endl;
        }

        std::thread tclient(clientHandler, client_fd);
        tclient.detach();	
    }

    return 0;
}	