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
#include <queue>
#include <list>
#include <mutex>
#include <chrono>
#include <set>

using namespace std;

mutex server_mutex;
mutex queue_mutex;
bool state = true;

struct server {
    struct sockaddr_in address;
    int server_fd;
    uint32_t k, p, noc;
    vector<string> words;
    set<int> closed_clients;
    server(string ip, uint16_t port, uint32_t k, uint32_t p, const string& filename, uint32_t noc) : k(k), p(p), server_fd(server_fd), noc(noc) {
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
    return server(j["server_ip"], static_cast<uint16_t>(j["server_port"]), j["k"], j["p"], j["input_file"], j["num_clients"].get<uint32_t>());
}

struct ThreadArgs {
    int client_socket;
    server* sv;
    int client_id;
};


struct Queue {
    list<pair<int, string>> message_queue;
    string protocol;
    vector<list<pair<int,string>>> fs_queue;
    unordered_map<int, int> client_id_to_index; 
    int ind=0;
    Queue() {}
    void push(int client_id, string message) {
        lock_guard<mutex> lock(queue_mutex);
        if(protocol == "fifo")
        message_queue.push_back({client_id, message});
        else{
            if(client_id_to_index.find(client_id) == client_id_to_index.end()){
                client_id_to_index[client_id] = fs_queue.size();
                fs_queue.push_back(list<pair<int, string>>());
            }
            fs_queue[client_id_to_index[client_id]].push_back({client_id, message});
        }
    }
    pair<int, string> pop() {
        lock_guard<mutex> lock(queue_mutex);
        if(protocol=="fifo"){
            pair<int, string> message = message_queue.front();
            message_queue.pop_front();
            return message;
        }
        else{
            while(fs_queue[ind].empty()){
                ind = (ind+1)%fs_queue.size();
            }
            pair<int, string> message = fs_queue[ind].front();
            fs_queue[ind].pop_front();
            ind = (ind+1)%fs_queue.size();
            return message;
        }
        
    }

    bool empty() {
        lock_guard<mutex> lock(queue_mutex);
        if(protocol == "fifo")
        return message_queue.empty();
        for(auto& q: fs_queue){
            if(q.size())return false;
        }
        return true;
    }
    void reset(const string& protocol){
        lock_guard<mutex> lock(queue_mutex);
        this->protocol = protocol;
        message_queue.clear();
        fs_queue.clear();
        client_id_to_index.clear();
        ind = 0;
    }
} packet_queue;

void* handle_client(void* args) {
    ThreadArgs* threadArgs = static_cast<ThreadArgs*>(args);
    int client_socket = threadArgs->client_socket;
    server& sv = *(threadArgs->sv);
    int client_id = threadArgs->client_id;
    delete threadArgs;

    while (sv.closed_clients.find(client_socket) == sv.closed_clients.end()) {
        char buffer[1024] = {0};
        int valread = read(client_socket, buffer, 1024);
        if (valread == 0) {
            break;
        } else if (valread < 0) {
            std::cerr << "Server: Failed to read" << client_id <<  std::endl;
            close(client_socket);
            return nullptr;
        }
        // cout<<"Server Received: "<<client_socket<<" "<<buffer<<endl;
        string messa = "";
        vector<string> messages;
        for (const char& c : buffer) {
            if (c == '\n') {
                messages.push_back(messa);
                messa.clear();
            }
            else messa.push_back(c);
        }
        for(const string& message:messages){
            packet_queue.push(client_socket, message);
        }
    }
    cout<<"Server Closing: "<<client_socket<<" "<<client_id<<endl;
    close(client_socket);
    return nullptr;
}

struct queue_handle_args {
    server* sv;
};

void* queue_handle(void* args){

    queue_handle_args *queue_args = static_cast<queue_handle_args*>(args);
    server& sv = *(queue_args->sv);

    while(state){
        if(packet_queue.empty()){
            continue;
        }
        pair<int, string> message = packet_queue.pop();
        int client_socket = message.first;
        string message_content = message.second;
        if(sv.closed_clients.find(client_socket) != sv.closed_clients.end()){
            continue;
        }
        // cout<<"Server Sending: "<<message.first<<" "<<message.second<<endl;
        vector<string> response = sv.response(stoi(message_content));
        for (const string& line : response) {
            // cout<<"Server Sending: "<<client_socket<<" "<<line<<endl;
            send(client_socket, line.c_str(), line.size(), 0);
            if(line.find("EOF\n") != string::npos){
                close(client_socket);
                lock_guard<mutex> lock(server_mutex);
                sv.closed_clients.insert(client_socket);
                // cout<<"Server Closing: "<<client_socket<<endl;
                break;
            }
        }
    }
    return nullptr;
}


int main(int argc, char* argv[]) {
    if(argc != 2){
        cout<<"Usage: ./server <protocol>"<<endl;
        return -1;
    }

    // packet_queue.protocol = argv[1];
    bool fairplay = std::strcmp(argv[1], "--fairness") == 0;
    bool plot = std::strcmp(argv[1], "--plot") == 0;
    bool all = plot || std::strcmp(argv[1], "--all") == 0;

    vector<pair<string,double>> time;
    server sv = read_parameters();

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
                vector<pthread_t> threads;
                sv.noc = plot?i:sv.noc;
                size_t users = sv.noc;
                packet_queue.reset(protocol);
                sv.closed_clients.clear();
                state = true;

                while (users) {
                    int addrlen = sizeof(sv.address);
                    int new_socket = accept(sv.server_fd, NULL,NULL);
                    if (new_socket < 0) {
                        std::cerr << "Server: Failed to accept" << std::endl;
                        continue;
                    }

                    ThreadArgs* args = new ThreadArgs{new_socket, &sv,11 - (int)users};
                    pthread_t thread;
                    pthread_create(&thread, nullptr, handle_client, args);
                    threads.push_back(thread);
                    users--;
                }
                pthread_t queue_thread;
                queue_handle_args queue_args{&sv};
                pthread_create(&queue_thread, nullptr, queue_handle, &queue_args);
                for (auto& thread : threads) {
                    pthread_join(thread, nullptr);
                }
                state = false;
                pthread_join(queue_thread, nullptr);
            }
        }
        close(sv.server_fd);
    }
    else{
        vector<string> protocols = {"fifo","rr"};
        for(size_t j=0;j<10;j++){
            for(string& protocol:protocols){
                cout<<protocol<<endl;
                vector<pthread_t> threads;
                sv.noc = 10;
                size_t users = sv.noc;
                packet_queue.reset(protocol);
                sv.closed_clients.clear();
                state = true;

                while (users) {
                    int new_socket = accept(sv.server_fd, NULL, NULL);
                    if (new_socket < 0) {
                        std::cerr << "Server: Failed to accept" << std::endl;
                        continue;
                    }
                    // if(users == 10)cout<<new_socket<<endl;
                    ThreadArgs* args = new ThreadArgs{new_socket, &sv,11 - (int)users};
                    pthread_t thread;
                    pthread_create(&thread, nullptr, handle_client, args);
                    threads.push_back(thread);
                    users--;
                }
                pthread_t queue_thread;
                queue_handle_args queue_args{&sv};
                pthread_create(&queue_thread, nullptr, queue_handle, &queue_args);
                for (auto& thread : threads) {
                    pthread_join(thread, nullptr);
                }
                cout<<"HI"<<endl;
                state = false;
                pthread_join(queue_thread, nullptr);
            }
        }
        close(sv.server_fd);
    }
    return 0;
}