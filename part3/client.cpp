#include <iostream>
#include "../json.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <unordered_map>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include <pthread.h>
#include <time.h>
#include <random>
#include <chrono>
#include <map>



using namespace std;
nlohmann::json json_data;
map<string, map<int,double>> time_taken;

struct ThreadArgs {
    size_t client_id;
    int server_socket;
    string protocol;
};

vector<string> read(int client_id, int offset){
    vector<string> response;
    string message = to_string(offset)+"\n";
    send(client_id, message.c_str(), message.size(), 0);
    
    int k = json_data["k"];
    while(k>0){
        char buffer[1024] = {0};
        int valread = read(client_id, buffer, 1024);
        if(valread == 0){
            break;
        }
        else if(valread == -1){
            cout<<"Error in reading"<<endl;
            break;
        }
        string temp;
        for(const char& c:buffer){
            if(c==','){response.push_back(temp);temp.clear();k--;}
            else if(c=='\n'){
                if(temp=="EOF"){
                    response.push_back(temp);
                    break;
                }
                else if(temp=="HUH!"){
                    response.clear();
                    response.push_back("HUH!");
                    return response;
                }
            }
            else temp.push_back(c);
        }
        if(temp=="EOF")break;
    }
    return response;
}

bool decide(long long int Tus, string protocol, int client_fd, uint32_t& collision_count){
    if(protocol == "aloha"){
        if(collision_count==0)return true;
        int noc = json_data["num_clients"];
        double p = 1.0/noc;
        int r = rand()%100;
        if(r<p*100){
            return true;
        }
        usleep(Tus);
        return false;
    }
    else if(protocol == "beb"){
        return true;
    }
    else{
        string message = "BUSY?\n";
        send(client_fd, message.c_str(), message.size(), 0);
        char buffer[1024] = {0};
        int valread = read(client_fd, buffer, 1024);
        string response;
        for(const char& c:buffer){
            if(c=='\n'){
                break;
            }
            response.push_back(c);
        }
        if(response == "IDLE"){
            return true;
        }
        usleep(Tus);
        return false;
    }
}

void collision(long long int Tus, string protocol, uint32_t& count){
    count++;
    if(protocol == "aloha")
    usleep(Tus);
    else{
        int wait = rand()%(1<<count);
        usleep(Tus*(wait));
    }

}

void* get_data(void* args){
    ThreadArgs* threadArgs = static_cast<ThreadArgs*>(args);
    size_t client_id = threadArgs->client_id;
    int client_fd = threadArgs->server_socket;
    string protocol = threadArgs->protocol;
    auto start = chrono::high_resolution_clock::now();

    double T = json_data["T"];
    long long int Tus = T*1000;
    
    unordered_map<string, size_t> word_count;
    uint32_t count = 0, collision_count = 0;
    bool eof=true;


    while(eof){
        if(!decide(Tus, protocol, client_fd, collision_count)) continue;
        vector<string> response = read(client_fd, count);
        if(response.empty()){
            continue;
        }
        else if(response[0]=="HUH!"){
            collision(Tus, protocol, collision_count);
            continue;
        }
        for(const string& word:response){   
            if(word=="EOF"){eof = false;break;}
            word_count[word]++;
            count++;
        }
        collision_count = 0;
    }

    close(client_fd);
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed = end - start;
    time_taken[protocol][json_data["num_clients"]]+=elapsed.count();
    return nullptr;
}


int create_server(){
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        std::cerr << "Client:Failed to create socket" << std::endl;
        exit(1);
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    uint16_t server_port = json_data["server_port"];
    serv_addr.sin_port = htons(server_port);//little endian to big endian
    string server_ip = json_data["server_ip"];
    serv_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());

    if(connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        std::cerr << "Client:Connection failed" << std::endl;
        exit(1);
    }

    return client_fd;

}

int main(int argc, char* argv[]){

    if(argc != 2){
        cout << "Usage: ./server <protocol>" << endl;
        return 1;
    }
    
    bool plot = std::strcmp(argv[1], "--plot") == 0;
    bool all = plot || std::strcmp(argv[1], "--all") == 0;
    
    ifstream jso("config.json");
    jso >> json_data;

    for(size_t i = 1,l = plot?32:1;i<=l;i++){
        vector<string> protocols;
        if(all){
            protocols = {"aloha", "beb", "csma"};
        }
        else{
            protocols.push_back(argv[1]);
        }
        for(string& protocol:protocols){
            sleep(1);
            cout<<"Protocol: "<<protocol<<" "<<i<<endl;

            vector<pthread_t> threads;
            size_t noc = plot?i:json_data["num_clients"].get<size_t>();
            json_data["num_clients"] = noc;
            time_taken[protocol][json_data["num_clients"]]=0;

            for(size_t i = 1;i<=noc;i++){
                int client_fd = create_server();

                ThreadArgs* args = new ThreadArgs{i, client_fd, protocol};
                pthread_t thread;
                pthread_create(&thread, nullptr, get_data, args);
                threads.push_back(thread);
            }

            for (pthread_t& thread : threads) {
                pthread_join(thread, nullptr);
            }
        }
    }
    if(!plot){
        for(auto& x:time_taken){
            for(auto& y:x.second)
            cout<<x.first<<", "<<y.second/y.first<<endl;
        }
    }
    else{
        ofstream output("temp.txt");
        for(const auto& x:time_taken){
            for(auto& y:x.second)
            output<<x.first<<", "<<y.first<<", "<<y.second/y.first<<endl;
        }
    }


    return 0;
}