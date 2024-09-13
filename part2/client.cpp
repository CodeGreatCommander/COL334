#include <iostream>
#include <nlohmann/json.hpp>
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

using namespace std;

mutex cout_mutex;
nlohmann::json json_data;

struct ThreadArgs {
    size_t client_id;
    int server_socket;
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
            if(c=='\n'||c==','){response.push_back(temp);temp.clear();k--;}
            else temp.push_back(c);
        }
    }
    return response;
}

void* get_data(void* args){
    ThreadArgs* threadArgs = static_cast<ThreadArgs*>(args);
    size_t client_id = threadArgs->client_id;
    int client_fd = threadArgs->server_socket;

    
    unordered_map<string, size_t> word_count;
    uint32_t count = 0;
    bool eof=true;
    while(eof){
        vector<string> response = read(client_fd, count);
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

    ofstream output("output/output_"+to_string((int)client_id)+".txt");
    for(const auto& [word, count]:word_count){
        output<<word<<' '<<count<<'\n';
    }
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

// void* client_duty(void* args) {
//     ThreadArgs* threadArgs = static_cast<ThreadArgs*>(args);
//     size_t client_id = threadArgs->client_id;
//     int server_socket = threadArgs->server_socket;
//     delete threadArgs; // Free the allocated memory

//     {
//         lock_guard<mutex> lock(cout_mutex);
//         cout << "Client " << to_string(client_id) << ": Connected to server" << endl;
//     }

//     size_t count = 0;
//     unordered_map<string, size_t> word_count;
//     bool end = false;
//     char buffer[1024] = {0};
//     int k = json_data["k"];
//     while (!end) {
//         string message = to_string(count) + "\n";
//         {
//             lock_guard<mutex> lock(cout_mutex);
//             cout << "Client " << client_id << ": <SENT> " << message << endl;
//         }
//         send(server_socket, message.c_str(), message.size(), 0);
//         int word_remaining = k;
//         while (word_remaining > 0 && !end) {
//             int valread = read(server_socket, buffer, 1024);
//             if (valread == 0) {
//                 end = true;
//                 break;
//             }
//             message = "";
//             for (const char& c : buffer) {
//                 if (c == '\n') break;
//                 message.push_back(c);
//             }
//             {
//                 lock_guard<mutex> lock(cout_mutex);
//                 cout << "Client " << client_id << ": <RECEIVED> " << message << endl;
//             }
//             string word = "";
//             for (const char& c : message) {
//                 if (c == ',') {
//                     word_count[word]++;
//                     word.clear();

//                     word_remaining--;
//                 } else {
//                     word.push_back(c);
//                 }
//             }
//             word_remaining--;
//             word_count[word]++;
//             if(word == "EOF")end =true;
//         }
//         count += k;
//     }
//     close(server_socket);
//     {
//         lock_guard<mutex> lock(cout_mutex);
//         cout << "Client " << client_id << ": Connection closed" << endl;
//     }
//     ofstream out("output/output_" + to_string(client_id) + ".txt");
//     for (const auto& [word, count] : word_count) {
//         out << word << "," << count << endl;
//     }
//     out.close();

//     return nullptr;
// }

int main() {
    ifstream jso("config.json");
    jso >> json_data;

    vector<pthread_t> threads;
    size_t noc = json_data["num_clients"];
    for (size_t i = 1; i <= noc ; i++) {
        // int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        // if (client_fd == -1) {
        //     std::cerr << "Client " << i << ": Failed to create socket" << std::endl;
        //     return -1;
        // }
        // struct sockaddr_in serv_addr;
        // serv_addr.sin_family = AF_INET;
        // uint16_t server_port = json_data["server_port"];
        // serv_addr.sin_port = htons(server_port); // little endian to big endian
        // string server_ip = json_data["server_ip"];
        // serv_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());

        // if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        //     std::cerr << "Client: Connection failed" << std::endl;
        //     i--;
        //     i = min(i, (size_t)1);
        //     continue;
        // }
        int client_fd = create_server();

        ThreadArgs* args = new ThreadArgs{i, client_fd};
        pthread_t thread;
        pthread_create(&thread, nullptr, get_data, args);
        threads.push_back(thread);
    }

    for (auto& thread : threads) {
        pthread_join(thread, nullptr);
    }

    return 0;
}