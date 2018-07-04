#ifndef __SOCKETS_H__
#define __SOCKETS_H__

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

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
    NonBlock = 0,
    Block,
  };

  enum class ReceivingStatus {
    CanReceive = 0,
    Receiving
  };
  
  enum class SendingStatus {
    CanSend = 0,
    Sending
  };

  struct Peer {
    Peer();
    Peer(const std::string& ip, uint32_t port);
    ~Peer();
    
    operator struct sockaddr*();
    
    static Peer LocalHost(uint32_t port = 14194);
    static Peer Any(uint32_t port = 14194);

    std::string ip_address;
    uint32_t port;
    bool ready;

  private:
    struct sockaddr_in address;
  };

  Socket();
  virtual ~Socket();

  bool bind(uint32_t port);
  bool close();
  uint32_t sendData(byte* buffer, uint32_t buffer_size);
  uint32_t receiveData(byte* buffer, uint32_t max_size_to_read);

protected:
  enum class ErrorFrom {
    SendData = 0,
    ReceiveData,
    Close
  };

  virtual void construct(Socket::Type type) = 0;
  virtual void handleError(ErrorFrom from, int32_t error) = 0;
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

  bool connect(const char* ip, uint32_t port);
  bool connect(const Peer& peer);
  bool isConnected() const;

private:
  friend class TCPListener;
  
  TCPSocket(Socket::Type type, uint32_t descriptor);
  TCPSocket();
  virtual void construct(Socket::Type type) override;
  virtual void handleError(ErrorFrom from, int32_t error) override;

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

  TCPListener(Socket::Type type, uint32_t queue_size = 32);
  ~TCPListener();

  bool listen();
  TCPSocket* accept();
  bool close();

private:
  TCPListener();
  virtual void construct(Socket::Type type) override;
  virtual void handleError(ErrorFrom from, int32_t error) override;

  ListeningStatus listening_status;
  TCPSocket* accepted_socket;
  uint32_t queue_size;
};

class UDPSocket : public Socket {
public:
  UDPSocket(Socket::Type type);
  ~UDPSocket();

  // TODO: method to broadcast
  uint32_t sendData(byte* buffer, uint32_t buffer_size, const Socket::Peer& peer);
  uint32_t receiveData(byte* buffer, uint32_t max_size_to_read, const Socket::Peer& peer);

private:
  UDPSocket();
  virtual void construct(Socket::Type type) override;
  virtual void handleError(ErrorFrom from, int32_t error) override;
};

#endif // __SOCKETS_H__