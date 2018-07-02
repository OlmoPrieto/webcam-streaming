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

  enum class ReceivingStatus {
    CanReceive,
    Receiving
  };
  
  enum class SendingStatus {
    CanSend,
    Sending
  };

  struct Peer;

  Socket();
  virtual ~Socket();

  bool bind(uint32_t port);
  bool close();
  bool isConnected() const;
  uint32_t sendData(byte* buffer, uint32_t buffer_size);
  uint32_t receiveData(byte* buffer, uint32_t max_size_to_read);

protected:
  virtual void construct(Socket::Type type) = 0;

  int32_t getDescriptor() const;

  struct sockaddr_in address;
  ReceivingStatus receiving_status;
  SendingStatus sending_status;
  Type type;
  int32_t socket_descriptor;
  bool closed;
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

  //bool bind(uint32_t port);
  bool connect(const char* ip, uint32_t port);
  uint32_t sendData(byte* buffer, uint32_t buffer_size);
  uint32_t receiveData(byte* buffer, uint32_t max_size_to_read);
  bool close();
  bool isConnected() const;

private:
  friend class TCPListener;
  
  TCPSocket(Socket::Type type, uint32_t descriptor);
  TCPSocket();
  void construct(Socket::Type type);
  int32_t getDescriptor() const;

  struct sockaddr_in address;
  Type type;
  ConnectionStatus connection_status;
  ReceivingStatus receiving_status;
  SendingStatus sending_status;
  int32_t socket_descriptor;
  bool closed;
};


class TCPListener : public Socket {
public:
  enum class ListeningStatus {
    NotListening,
    Listening,
    WaitingForAccept,
    Accepted
  };

  TCPListener(Socket::Type type, uint32_t queue_size = 32);
  ~TCPListener();

  bool bind(uint32_t port);
  bool listen();
  TCPSocket* accept();
  bool close();
  bool closeConnection(uint32_t target_connection);

private:
  TCPListener();
  void construct(Socket::Type type);

  struct sockaddr_in address;
  Type type;
  ListeningStatus listening_status;
  TCPSocket* accepted_socket;
  uint32_t queue_size;
  int32_t socket_descriptor;
  bool closed;
};

class UDPSocket : public Socket {
public:
  uint32_t sendData(byte* buffer, uint32_t buffer_size, const Socket::Peer& peer);
  uint32_t receiveData(byte* buffer, uint32_t max_size_to_read, const Socket::Peer& peer);
};

#endif // __SOCKETS_H__