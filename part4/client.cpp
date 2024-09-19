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
#include <chrono>

using namespace std;

mutex cout_mutex;
nlohmann::json json_data;

map<string, map<int,double>> time_taken;
map<int, double> fairness;

struct ThreadArgs {
    size_t client_id;
    int server_socket;
    string protocol;
    bool rogue;
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
        string temp;
        for(const char& c:buffer){
            if(c==','){response.push_back(temp);temp.clear();k--;}
            else if(c=='\n'){
                if(temp=="EOF"){
                    response.push_back(temp);
                    break;
                }
            }
            else temp.push_back(c);
        }
        if(temp=="EOF")break;
    }
    return response;
}
vector<string> rogue_read(int client_id, int offset){
    vector<string> response;
    int k = json_data["k"];
    for(int i=0;i<5;i++){
        string message = to_string(offset+i*k)+"\n";
        send(client_id, message.c_str(), message.size(), 0);
    }
    while(k>0){
        char buffer[1024] = {0};
        int valread = read(client_id, buffer, 1024);
        if(valread == 0){
            break;
        }
        k*=5;
        string temp;
        for(const char& c:buffer){
            if(c==','){response.push_back(temp);temp.clear();k--;}
            else if(c=='\n'){
                if(temp=="EOF"){
                    response.push_back(temp);
                    break;
                }
            }
            else temp.push_back(c);
        }
        if(temp=="EOF")break;
    }
    return response;
}

void* get_data(void* args){
    ThreadArgs* threadArgs = static_cast<ThreadArgs*>(args);
    size_t client_id = threadArgs->client_id;
    int client_fd = threadArgs->server_socket;
    string protocol = threadArgs->protocol;
    bool rogue = threadArgs->rogue;

    delete threadArgs;
    
    auto start = chrono::high_resolution_clock::now();

    unordered_map<string, size_t> word_count;
    uint32_t count = 0;
    bool eof=true;
    while(eof){
        vector<string> response;
        if(rogue) response = rogue_read(client_fd, count);
        else response = read(client_fd, count);
        if(response.empty()){
            continue;
        }
        for(const string& word:response){
            if(word=="EOF"){eof = false;break;}
            word_count[word]++;
            count++;
        }
    }

    close(client_fd);
    cout<<"Client closed"<< client_fd<<" "<<client_id << endl;

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed = end - start;
    time_taken[protocol][json_data["num_clients"]]+=elapsed.count();
    fairness[client_id] = elapsed.count();

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

int main(int argc, char* argv[]) {

    if(argc != 2){
        cout << "Usage: ./server <protocol>" << endl;
        return 1;
    }
    
    bool plot = std::strcmp(argv[1], "--plot") == 0;
    bool all = plot || std::strcmp(argv[1], "--all") == 0;
    bool fairplay = std::strcmp(argv[1], "--fairness") == 0;
    ifstream jso("config.json");
    jso >> json_data;

    if(!fairplay){
        for(size_t i = 1,l = plot?32:1;i<=l;i++){
            vector<string> protocols;
            if(all){
                protocols = {"fifo","rr"};
            }
            else{
                protocols.push_back(argv[1]);
            }
            for(string& protocol:protocols){
                sleep(1);    
                vector<pthread_t> threads;
                size_t noc = plot?i:json_data["num_clients"].get<size_t>();
                json_data["num_clients"] = noc;
                cout<<"Protocol: "<<protocol<<" "<<noc<<endl;

                time_taken[protocol][json_data["num_clients"]]=0;

                for (size_t i = 1; i <= noc ; i++) {
                    
                    int client_fd = create_server();

                    ThreadArgs* args = new ThreadArgs{i, client_fd, protocol, false};
                    pthread_t thread;
                    pthread_create(&thread, nullptr, get_data, args);
                    threads.push_back(thread);
                }

                for (auto& thread : threads) {
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
    }
    else{
        vector<string> protocols = {"fifo","rr"};
        double fi=0,rr=0;
        for(size_t j=0;j<10;j++){
            for(string& protocol:protocols){
                sleep(1);    
                vector<pthread_t> threads;
                size_t noc = 10;
                json_data["num_clients"] = noc;
                cout<<"Protocol: "<<protocol<<" "<<noc<<endl;

                time_taken[protocol][json_data["num_clients"]]=0;


                int client_fd = create_server();
                int rogue = (rand()%10)+1;
                cout<<"Rogue"<<rogue<<endl;

                for (size_t i = 1; i <= noc ; i++) {
                    
                    int client_fd = create_server();

                    ThreadArgs* args = new ThreadArgs{i, client_fd, protocol, i==rogue};
                    pthread_t thread;
                    pthread_create(&thread, nullptr, get_data, args);
                    threads.push_back(thread);
                }

                for (auto& thread : threads) {
                    pthread_join(thread, nullptr);
                }

                double sum, sum_sq;
                sum = sum_sq = 0;
                for(auto& x:fairness){
                    sum+=1.0/x.second;
                    sum_sq+=1.0/(x.second*x.second);
                }
                cout<<"Jain's Fariness Index: "<<sum*sum/(10*sum_sq)<<endl;
                if(protocol=="fifo")fi+=sum*sum/(10*sum_sq);
                else rr+=sum*sum/(10*sum_sq);
                fairness.clear();
            }
        }
        cout<<"FIFO: "<<fi/10<<endl;
        cout<<"RR: "<<rr/10<<endl;
    }
    return 0;
}