#include <iostream>
#include <nlohmann/json.hpp>
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


using namespace std;

struct server{
    struct sockaddr_in address;
    int server_fd;
    uint32_t k, p;
    vector<string> words;
    server(string ip, uint16_t port, uint32_t k, uint32_t p, const string& filename,int server_fd): k(k), p(p), server_fd(server_fd){
        address.sin_family = AF_INET;
        address.sin_port = htons(port);//little endian to big endian
        address.sin_addr.s_addr = inet_addr(ip.c_str());
        string_parsing(filename);
    }

    void string_parsing(const string& filename){
        ifstream file_input(filename);
        string line;
        while(getline(file_input, line)){
            stringstream ss(line);
            string word;
            while(ss >> word){
                words.push_back(word);
            }
        }
        words.push_back("EOF");
    }

    vector<string> response(size_t offset){
        vector<string> response;
        string temp;
        for(size_t i=offset;i<min(offset+k,words.size());i++){
            temp+=words[i];
            if((i-offset+1)%p==0 || i==words.size()-1){
                temp.push_back('\n');
                response.push_back(temp);
                temp.clear();
            }
            else temp.push_back(',');
        }
        if(!temp.empty()){
            response.push_back(temp);
        }
        return response;
    }

};

server read_parameters(int& server_fd){
    ifstream i("config.json");
    nlohmann::json j;
    i >> j;
    return server(j["server_ip"],static_cast<uint16_t>(j["server_port"]), j["k"], j["p"], j["filename"], server_fd);
}


int main(){

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Server:Failed to create socket" << std::endl;
        return -1;
    }

    server sv = read_parameters(server_fd);

    if(bind(sv.server_fd, (struct sockaddr*)&sv.address, sizeof(sv.address)) < 0){
        std::cerr << "Server:Failed to bind" << std::endl;
        return -1;
    }

    if(listen(sv.server_fd, 1) < 0){
        std::cerr << "Server:Failed to listen" << std::endl;
        return -1;
    }


    int addrlen = sizeof(sv.address);
    int new_socket = accept(sv.server_fd, (struct sockaddr*)&sv.address, (socklen_t*)&addrlen);
    if(new_socket < 0){
        std::cerr << "Server:Failed to accept" << std::endl;
        close(sv.server_fd);
        return -1;
    }

    char buffer[1024] = {0};
    size_t read_count = 0;
    while(read_count<sv.words.size()){
        int valread = read(new_socket, buffer, 1024);
        if(valread == 0){
            break;
        }
        else if(valread<0){
            std::cerr << "Server:Failed to read" << std::endl;
            close(new_socket);
            close(sv.server_fd);
            return -1;
        }
        string message = "";
        bool end = false;
        for(const char& c:buffer){
            if(c=='\n'){
                end = true;
                break;
            }
            message.push_back(c);
        }
        if(!end)continue;
        vector<string> responses=sv.response(stoi(message));
        for(const string& response:responses){
            send(new_socket, response.c_str(), response.size(), 0);
        }
        read_count+=sv.k;
    }
    close(new_socket);
    cout<<"Server: Connection closed"<<endl;
    close(sv.server_fd);
    cout<<"Server: Socket closed"<<endl;


    return 0;
}