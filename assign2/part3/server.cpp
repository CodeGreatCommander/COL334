#include <iostream>
#include <fstream>
#include "../json.hpp"
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

using namespace std;

uint32_t k = 10, p=1;


struct server {
    struct sockaddr_in address;
    int server_fd;
    uint32_t k, p, noc;//noc: number of clients
    vector<string> words;
    bool collision, use;
    server(string ip, uint16_t port, uint32_t k, uint32_t p, const string& filename, uint32_t noc) : k(k), p(p), server_fd(server_fd), collision(false), use(false), noc(noc) {
        //setup server
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            std::cerr << "Server:Failed to create socket" << std::endl;
            throw runtime_error("Server:Failed to create socket");
        }

        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set SO_REUSEADDR" << std::endl;
            close(server_fd);
            throw runtime_error("Failed to set SO_REUSEADDR");
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
    return server(j["server_ip"], static_cast<uint16_t>(j["server_port"]), k, p, j["input_file"], j["num_clients"]);
}

string read_message(char buffer[1024]){
    string message;
    for(int i=0; i<1024; i++){
        if(buffer[i] == '\n')return message;
        message.push_back(buffer[i]);
    }
    return message;
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
    delete threadArgs;

    bool eof = false;
    while(!eof){
        char buffer[1024]={0};
        int valread = read(client_socket, buffer, 1024);
        if(valread == 0){
            break;
        }
        string message=read_message(buffer);
        if(message == "BUSY?"){
            if(sv.use){
                message = "BUSY\n";
                send(client_socket, message.c_str(), message.size(), 0);
            }
            else{
                // cout<<"Server:IDLE"<<endl;
                message = "IDLE\n";
                send(client_socket, message.c_str(), message.size(), 0);
            }
            continue;
        }
        if(sv.use){
            sv.collision=true;
            message = "HUH!\n";
            send(client_socket, message.c_str(), message.size(), 0);
            continue;
        }
        sv.use = true;
        vector<string> response = sv.response(stoi(message));
        for(const string& s: response){
            if(sv.collision){
                // cout<<"Server:HUH"<<endl;
                message = "HUH!\n";
                send(client_socket, message.c_str(), message.size(), 0);
                sv.collision = false;
                sv.use = false;
                break;
            }
            // cout<<"Server:"<<s;
            send(client_socket, s.c_str(), s.size(), 0);
        }
        if(sv.use){
            sv.use = false;
            if(response.size()&&response.back()=="EOF\n"){
                eof = true;
            }
        }
    }
    close(client_socket);
    return nullptr;
}


int main(int argc, char* argv[]) {;
    if(argc != 2){
        cout << "Usage: ./server <protocol>" << endl;
        return 1;
    }
    string protocol = argv[1];

    server sv = read_parameters();

    bool plot = std::strcmp(argv[1], "--plot") == 0;
    bool all = plot || std::strcmp(argv[1], "--all") == 0;
    
    vector<pair<string,double>> time;

    for(size_t i = 1,l = plot?32:1;i<=l;i++){
        vector<string> protocols;
        if(all){
            protocols = {"aloha", "beb", "csma"};
        }
        else{
            protocols.push_back(argv[1]);
        }
        for(string& protocol:protocols){
            
            vector<pthread_t> threads;
            sv.noc = plot?i:sv.noc;
            size_t users = sv.noc;

            while (users) {
                int addrlen = sizeof(sv.address);
                int new_socket = accept(sv.server_fd, NULL, NULL);
                if (new_socket < 0) {
                    std::cerr << "Server: Failed to accept" << std::endl;
                    continue;
                }
                // cout<<"Server: Accepted connection with client "<<sv.noc - users + 1<<endl;
                ThreadArgs* args = new ThreadArgs{new_socket, &sv,(int)sv.noc + 1 - (int)users};
                pthread_t thread;
                pthread_create(&thread, nullptr, handle_client, args);
                threads.push_back(thread);
                users--;
            }

            for (auto& thread : threads) {
                pthread_join(thread, nullptr);
            }
        }
    }
    close(sv.server_fd);

    return 0;

}