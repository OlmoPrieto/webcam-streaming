#include <iostream>
#include <cstring>
#include <cerrno>

#include "sockets.h"

// TODO: now there are two versions of sockets.h and sockets.cpp, 
// unify in a Base project with only include and src folders

int main() {
  TCPListener listener(Socket::Type::Block, 128);
  listener.bind(14194);
  listener.listen();
  TCPSocket* socket = nullptr;
  while (!socket) {
    socket = listener.accept();
  }
  printf("Accepted connection\n");

  byte* data = (byte*)malloc(600000);
  int32_t read = socket->receiveData(data, sizeof(data));
  if (read > 0) {
    printf("Data received\n");
    printf("data[1] = %u\n", data[1]);
  }

  // while (true) {
    
  // }
  socket->close();

  return 0;
}