#include "sockets.h"

#include <cassert>

// [TCPSocket]
TCPSocket::TCPSocket(Type _type) {
  type = _type;
  TCPSocket();
}

TCPSocket::~TCPSocket() {
  close();
}

bool TCPSocket::bind(uint32_t port) {
  address.sin_port = htons(port);

  return (bool)::bind(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
}

bool TCPSocket::connect(const char* ip) {
  address.sin_addr.s_addr = inet_addr(ip);
  assert(address.sin_addr.s_addr != 0 && address.sin_port != 0);
  return (bool)::connect(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
}

void TCPSocket::sendData(byte* buffer, uint32_t buffer_size) {
  ::send(socket_descriptor, buffer, buffer_size, 0);
}

uint32_t TCPSocket::receiveData(byte* buffer, uint32_t max_size_to_read) {
  return recv(socket_descriptor, buffer, max_size_to_read, 0);
}

bool TCPSocket::close() {
  return (bool)::close(socket_descriptor);
}
  
/*private*/TCPSocket::TCPSocket(Type type, uint32_t descriptor) {
  socket_descriptor = descriptor;

  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int*)1, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  int32_t flags = type == Type::NonBlock ? 0 : O_NONBLOCK;
  fcntl(socket_descriptor, F_SETFL, flags);
}

/*private*/TCPSocket::TCPSocket() {
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;

  // Should put this in an init() method
  socket_descriptor = socket(address.sin_family, SOCK_STREAM, 0);
  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int*)1, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  int32_t flags = type == Type::NonBlock ? 0 : O_NONBLOCK;
  fcntl(socket_descriptor, F_SETFL, flags);
}

/*private*/int32_t TCPSocket::getDescriptor() const {
  return socket_descriptor;
}
// [\TCPSocket]



// [TCPListener]
TCPListener::TCPListener(Type _type, uint32_t _queue_size) {
  type = _type;
  queue_size = _queue_size;
  TCPListener();
}

TCPListener::~TCPListener() {
  close();
}

bool TCPListener::bind(uint32_t port) {
  address.sin_addr.s_addr = htons(INADDR_ANY);
  address.sin_port = htons(port);

  return (bool)::bind(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
}

bool TCPListener::listen() {
  return (bool)::listen(socket_descriptor, queue_size);
}
  
TCPSocket* TCPListener::accept() {
  int accepted_socket_des = ::accept(socket_descriptor, nullptr, nullptr);
  if (accepted_socket_des >= 0) {
    accepted_socket = new TCPSocket(type, accepted_socket_des);
  }

  return accepted_socket;
}

bool TCPListener::close() {
  bool success = true;

  if (accepted_socket) {
    success = (bool)shutdown(accepted_socket->getDescriptor(), SHUT_RDWR);
    delete accepted_socket;
  }
  
  success = (bool)::close(socket_descriptor) | success;

  return success;
}

/*private*/TCPListener::TCPListener() {
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;

  // Should put this in an init() method
  socket_descriptor = socket(address.sin_family, SOCK_STREAM, 0);
  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int*)1, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  int32_t flags = type == Type::NonBlock ? 0 : O_NONBLOCK;
  fcntl(socket_descriptor, F_SETFL, flags);
}
// [\TCPListener]