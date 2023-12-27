# Lab6_Socket
server.cpp需要在类unix平台编译，`g++ ./server/server.cpp -o ./server/server.exe -pthread`
client.cpp需要在Windows平台编译，`g++ -std=c++11 ./client/client.cpp -o ./client/client.exe -lwsock32`