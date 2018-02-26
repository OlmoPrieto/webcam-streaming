#include <iostream>
#include <cstring>

#include "sockets.h"

int main() {
  TCPSocket socket(Socket::Type::Block);
  socket.bind(14194);
  socket.connect("127.0.0.1");
  byte* data = (byte*)malloc(600000);
  memset(data, 1, 600000);
  socket.sendData(data, 600000);
  socket.close();

  return 0;
}