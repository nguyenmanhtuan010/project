#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <chrono>

#define PORT 5000
#define BUFFER_SIZE 1024

std::vector<int> clients;

bool isFileEmpty(const char* filename) {
    std::ifstream file(filename);
    return file.peek() == std::ifstream::traits_type::eof();
}

void writeFile(int clientSocket, const char* filename, const char* content) {
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) {
        send(clientSocket, "Failed to open file\n", strlen("Failed to open file\n"), 0);
    } else {
        file << content;
        file.close();
        send(clientSocket, "File updated\n", strlen("File updated\n"), 0);
    }
}

void tailFile(int clientSocket, const char* filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        send(clientSocket, "Failed to open file\n", strlen("Failed to open file\n"), 0);
        return;
    }

    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content;
    char buffer[BUFFER_SIZE];

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for 500ms before checking for changes

        file.clear();
        file.seekg(0, std::ios::end);
        std::streampos newSize = file.tellg();

        if (newSize > fileSize) {
            file.seekg(fileSize);
            while (file.getline(buffer, BUFFER_SIZE)) {
                content += buffer;
                content += "\n";
            }

            send(clientSocket, content.c_str(), content.length(), 0);
            content.clear();

            fileSize = newSize;
        }
    }

    file.close();
}

void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE] = {0};
    while (true) {
        int valread = read(clientSocket, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            break;
        }

        char cleanedBuffer[BUFFER_SIZE] = {0};
        int j = 0;
        for (int i = 0; i < valread; ++i) {
            if (buffer[i] != '\n') {
                cleanedBuffer[j++] = buffer[i];
            }
        }

        if (strncmp(cleanedBuffer, "create ", 7) == 0) {
            std::ofstream file(&cleanedBuffer[7]);
            if (!file.is_open()) {
                send(clientSocket, "Failed to create file\n", strlen("Failed to create file\n"), 0);
            } else {
                send(clientSocket, "File created\n", strlen("File created\n"), 0);
                file.close();
            }
        } else if (strncmp(cleanedBuffer, "read ", 5) == 0) {
            char* filename = &cleanedBuffer[5];
            std::ifstream file(filename);
            if (!file.is_open()) {
                send(clientSocket, "Failed to open file\n", strlen("Failed to open file\n"), 0);
            } else {
				if (isFileEmpty(filename)) {
				char temp[] = "empty";
				send(clientSocket, temp, strlen(temp), 0);
				}else{
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                send(clientSocket, content.c_str(), content.length(), 0);
                file.close();}
            }
        } else if (strncmp(cleanedBuffer, "delete ", 7) == 0) {
            if (std::remove(&cleanedBuffer[7]) != 0) {
                send(clientSocket, "Failed to delete file\n", strlen("Failed to delete file\n"), 0);
            } else {
                send(clientSocket, "File deleted\n", strlen("File deleted\n"), 0);
            }
        } else if (strncmp(cleanedBuffer, "write ", 6) == 0) {
            char* filename = strtok(&cleanedBuffer[6], " ");
            char* content = strtok(NULL, "");
            if (filename != NULL && content != NULL) {
                writeFile(clientSocket, filename, content);
            } else {
                send(clientSocket, "Invalid write command\n", strlen("Invalid write command\n"), 0);
            }
        } else if (strncmp(cleanedBuffer, "tail ", 5) == 0) {
            char* filename = &cleanedBuffer[5];
            tailFile(clientSocket, filename);
        } else {
            send(clientSocket, "Invalid command\n", strlen("Invalid command\n"), 0);
        }
    }

    close(clientSocket);
    auto it = std::remove(clients.begin(), clients.end(), clientSocket);
    clients.erase(it, clients.end());
}

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return -1;
    }

    if (listen(serverSocket, 3) < 0) {
        std::cerr << "Listen failed" << std::endl;
        return -1;
    }

    while (true) {
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }

        clients.push_back(clientSocket);
        std::thread(handleClient, clientSocket).detach();
    }

    return 0;
}
