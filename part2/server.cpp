#include <iostream>
#include "../json.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <mutex>

using namespace std;

mutex cout_mutex;

struct server {
    struct sockaddr_in address;
    int server_fd;
    uint32_t k, p, num_users;
    vector<string> words;
    server(string ip, uint16_t port, uint32_t k, uint32_t p, const string& filename, uint32_t num_users) : k(k), p(p), server_fd(server_fd), num_users(num_users) {
        //setup server
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            std::cerr << "Server:Failed to create socket" << std::endl;
            throw runtime_error("Server:Failed to create socket");
        }
        address.sin_family = AF_INET;
        address.sin_port = htons(port);//little endian to big endian
        address.sin_addr.s_addr = inet_addr(ip.c_str());

        //bind to the port and listen
        if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0){
            std::cerr << "Server:Failed to bind" << std::endl;
            close(server_fd);
            throw runtime_error("Server:Failed to bind");
        }

        if(listen(server_fd, 1) < 0){
            std::cerr << "Server:Failed to listen" << std::endl;
            close(server_fd);
            throw runtime_error("Server:Failed to listen");
        }
        string_parsing(filename);
    }

    void string_parsing(const string& filename){
        ifstream file_input(filename);
        string line;
        while(getline(file_input, line)){
            stringstream ss(line);
            string word;
            while(getline(ss, word, ',')){
                word.push_back(',');
                words.push_back(word);
            }
        }
    }

    vector<string> response(size_t offset){
        vector<string> response;
        string temp;
        for(size_t i=offset;i<min(offset+k,words.size());i++){
            temp+=words[i];
            if(i==words.size()-1){
                temp.append("EOF\n");
                response.push_back(temp);
                temp.clear();
            }
            else if((i-offset+1)%p==0){
                temp.push_back('\n');
                response.push_back(temp);
                temp.clear();
            }
        }
        if(!temp.empty()){
            temp.push_back('\n');
            response.push_back(temp);
        }
        return response;
    }
};

server read_parameters() {
    ifstream i("config.json");
    nlohmann::json j;
    i >> j;
    return server(j["server_ip"], static_cast<uint16_t>(j["server_port"]), j["k"], j["p"], j["input_file"], j["num_clients"]);
}

struct ThreadArgs {
    int client_socket;
    server* sv;
    int client_id;
};

void* handle_client(void* args) {
    ThreadArgs* threadArgs = static_cast<ThreadArgs*>(args);
    int client_socket = threadArgs->client_socket;
    server& sv = *(threadArgs->sv);
    int client_id = threadArgs->client_id;
    delete threadArgs; // Free the allocated memory

    char buffer[1024] = {0};
    size_t read_count = 0;
    while (read_count < sv.words.size()) {
        int valread = read(client_socket, buffer, 1024);
        if (valread == 0) {
            break;
        } else if (valread < 0) {
            std::cerr << "Server: Failed to read" << std::endl;
            close(client_socket);
            close(sv.server_fd);
            return nullptr;
        }
        string message = "";
        bool end = false;
        for (const char& c : buffer) {
            if (c == '\n') {
                end = true;
                break;
            }
            message.push_back(c);
        }
        if (!end) continue;
        // {
        //     lock_guard<mutex> lock(cout_mutex);
        //     cout << "Server: <RECEIVED CLIENT " << client_id << ">: " << message << endl;
        // }
        vector<string> responses = sv.response(stoi(message));
        for (const string& response : responses) {
            // {
            //     lock_guard<mutex> lock(cout_mutex);
            //     cout<< sv.p <<endl;
            //     cout << "Server: <SENT CLIENT " << client_id << ">: " << response << endl;
            // }
            send(client_socket, response.c_str(), response.size(), 0);
        }
        read_count += sv.k;
    }
    close(client_socket);
    // {
    //     lock_guard<mutex> lock(cout_mutex);
    //     cout << "Server (Client " << client_id << "): Connection closed" << endl;
    // }

    return nullptr;
}

int main(int argc, char* argv[]) {

    bool plot = false;
    if (argc == 2 && std::strcmp(argv[1], "--plot") == 0) {
        plot = true;
    }
    
    vector<double> time;
    server sv = read_parameters();


    for(size_t i = 1,l = plot?32:1;i<=l;i++){
        vector<pthread_t> threads;
        

        size_t users;
        if(plot)users = i;
        else users = sv.num_users;

        while (users) {
            int addrlen = sizeof(sv.address);
            int new_socket = accept(sv.server_fd, (struct sockaddr*)&sv.address, (socklen_t*)&addrlen);
            if (new_socket < 0) {
                std::cerr << "Server: Failed to accept" << std::endl;
                continue;
            }
            // cout << "Server: New connection accepted with client " << sv.num_users+1 - users << endl;
            int client_id = static_cast<int>(sv.num_users+1 - users);
            ThreadArgs* args = new ThreadArgs{new_socket, &sv, client_id};
            pthread_t thread;
            pthread_create(&thread, nullptr, handle_client, args);
            threads.push_back(thread);
            users--;
        }

        for (auto& thread : threads) {
            pthread_join(thread, nullptr);
        }
    }
    close(sv.server_fd);
    // cout << "Server: Socket closed" << endl;

    return 0;
}