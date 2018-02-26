#include <iostream>
#include <cstring>

#include "sockets.h"

// TODO: now there are two versions of sockets.h and sockets.cpp, 
// unify in a Base project with only include and src folders

int main() {
  TCPListener listener(Socket::Type::NonBlock);
  printf("Created listener\n");
  listener.bind(14194);
  printf("Listener bound\n");
  listener.listen();
  printf("Listener listening\n");
  TCPSocket* socket = nullptr;
  while (!socket) {
    printf("WJGHKJASGH\n");
    socket = listener.accept();
  }
  printf("Accepted connection\n");

  byte* data = (byte*)malloc(600000);
  uint32_t read = socket->receiveData(data, sizeof(data));
  if (read > 0) {
    printf("Data received\n");
  }

  socket->close();

  return 0;
}