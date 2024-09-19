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
#include <chrono>


using namespace std;

struct server{
    struct sockaddr_in address;
    int server_fd;
    uint32_t k, p;
    vector<string> words;

    server(string ip, uint16_t port, uint32_t k, uint32_t p, const string& filename): k(k), p(p){
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

server read_parameters(int argc, char* argv[]){
    ifstream i("config.json");
    nlohmann::json j;
    i >> j;
    return server(j["server_ip"],static_cast<uint16_t>(j["server_port"]), j["k"], j["p"], j["input_file"]);
}


int main(int argc, char* argv[]){
    bool plot = false;
    if (argc == 2 && std::strcmp(argv[1], "--plot") == 0) {
        plot = true;
    }

    server sv = read_parameters(argc, argv);

    vector<double> time;
    uint32_t p = 1;
    do{
        for(size_t _ = 0, _e = plot?10:1; _ < _e;_++){
            if(plot){
                sv.p = p;
                sv.k = 10;
            }
            int addrlen = sizeof(sv.address);
            int new_socket = accept(sv.server_fd, NULL, NULL);
            if(new_socket < 0){
                std::cerr << "Server:Failed to accept" << std::endl;
                close(sv.server_fd);
                return -1;
            }
            auto start = chrono::high_resolution_clock::now();

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
            auto end = chrono::high_resolution_clock::now();
            time.push_back(chrono::duration_cast<chrono::microseconds>(end - start).count());
        }
        p++;
    }while(plot && p<=10);


    close(sv.server_fd);

    if(plot){
        ofstream output("temp.txt");
        for(const double& t:time){
            output<<t<<'\n';
        }
    }

    return 0;
}