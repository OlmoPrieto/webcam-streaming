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
  enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Ready
  };

  TCPSocket(Type type);
  ~TCPSocket();

  bool bind(uint32_t port);
  bool connect(const char* ip, uint32_t port);
  void sendData(byte* buffer, uint32_t buffer_size);
  uint32_t receiveData(byte* buffer, uint32_t max_size_to_read);
  bool close();

private:
  friend class TCPListener;
  
  TCPSocket(Type type, uint32_t descriptor);
  TCPSocket();
  void construct();
  int32_t getDescriptor() const;

  struct sockaddr_in address;
  Type type;
  int32_t socket_descriptor;
  ConnectionStatus connection_status;
};


class TCPListener : public Socket {
public:
  enum class ListeningStatus {
    NotListening,
    Listening,
    WaitingForAccept,
    Accepted
  };

  TCPListener(Type type, uint32_t queue_size = 32);
  ~TCPListener();

  bool bind(uint32_t port);
  bool listen();
  TCPSocket* accept();
  bool close();
  bool closeConnection(uint32_t target_connection);

private:
  TCPListener();
  void construct();

  struct sockaddr_in address;
  Type type;
  TCPSocket* accepted_socket;
  uint32_t queue_size;
  int32_t socket_descriptor;
  ListeningStatus listening_status;
};

#endif // __SOCKETS_H__