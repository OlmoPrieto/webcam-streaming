#include <iostream>
#include <cstring>
#include <cerrno>

#include "sockets.h"
#include "chrono.h"

// TODO: now there are two versions of sockets.h and sockets.cpp, 
// unify in a Base project with only include and src folders

int main() {
  float acc_time = 0.0f;
  Chrono c;
  c.start();

  TCPListener listener(Socket::Type::NonBlock, 128);
  listener.bind(14194);
  listener.listen();
  c.stop();
  acc_time += c.timeAsMilliseconds();
  printf("Time to listen: %.2f ms\n", c.timeAsMilliseconds());
  c.start();
  TCPSocket* socket = nullptr;
  while (!socket) {
    socket = listener.accept();
  }
  printf("Accepted connection\n");
  c.stop();
  acc_time += c.timeAsMilliseconds();
  printf("Time to accept program: %.2f ms\n", c.timeAsMilliseconds());
  c.start();

  uint32_t buffer_size = 128;
  byte* data = (byte*)malloc(buffer_size);
  uint32_t read = 0;
  while (read == 0) {
    read = socket->receiveData(data, sizeof(data));
  }
  printf("Data received\n");
  printf("data[1] = %u\n", data[1]);

  // while (true) {

  // }
  socket->close();

  c.stop();
  acc_time += c.timeAsMilliseconds();
  printf("Time to receive data program: %.2f ms\n", c.timeAsMilliseconds());

  printf("Time to run program: %.2f ms\n", acc_time);

  return 0;
}