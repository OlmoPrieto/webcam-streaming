#ifndef __SOCKETS_H__
#define __SOCKETS_H__

#include <vector>
#include <cstdint>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef unsigned char byte;

class Socket {
public:
  enum class Type {
    NonBlock,
    Block,
  };
};

class TCPSocket : public Socket {
public:
  TCPSocket(Type type);
  ~TCPSocket();

  bool bind(uint32_t port);
  bool connect(const char* ip);
  void sendData(byte* buffer, uint32_t buffer_size);
  bool close();

private:
  friend class TCPListener;
  
  TCPSocket(Type type, uint32_t descriptor);
  TCPSocket();

  int32_t getDescriptor() const;

  struct sockaddr_in address;
  Type type;
  int32_t socket_descriptor;
};


class TCPListener : public Socket {
public:
  TCPListener(Type type, uint32_t queue_size = 32);
  ~TCPListener();

  bool bind(uint32_t port);
  bool listen();
  TCPSocket* accept();
  bool close();
  bool closeConnection(uint32_t target_connection);

private:
  TCPListener();

  struct sockaddr_in address;
  Type type;
  TCPSocket* accepted_socket;
  uint32_t queue_size;
  int32_t socket_descriptor;
};

#endif // __SOCKETS_H__