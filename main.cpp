#include "raft.h"
int main()
{
    std::vector<netArgs> vec;
    struct netArgs arg1, arg2, arg3;
    arg1 = {"127.0.0.1", "8080"};
    arg2 = {"127.0.0.1", "8081"};
    arg3 = {"127.0.0.1", "8082"};

    vec.push_back(arg1);
    vec.push_back(arg2);
    vec.push_back(arg3);
    int id = 0;

    struct netArgs args;
    std::string port_ = "";
    std::cout << " cin for port ";
    std::cin >> port_;
    std::cout << std::endl;
    std::cout << " cin for nodeID ";
    std::cin >> id;
    std::cout << std::endl;
    args = {"127.0.0.1", port_};
    RaftNode node1(args, id, vec);
    node1.StartService();
}