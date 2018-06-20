#include "sockets.h"

#include <cassert>
#include <cerrno>
#include <cstdio>

#include <netinet/tcp.h>


#define IGNORE_PRINTF 0
#if IGNORE_PRINTF == 1
  #define printf(fmt, ...) (0)
#endif

#define error_printf(fmt, ...) (printf(fmt, ##__VA_ARGS__))
#define IGNORE_ERROR_PRINTF 1
#if IGNORE_ERROR_PRINTF == 1
  #undef error_printf
  #define error_printf(fmt, ...) (0)
#endif

// [TCPSocket]
TCPSocket::TCPSocket(Type _type) {
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
                            // AF_INET
  socket_descriptor = socket(address.sin_family, SOCK_STREAM, 0);

  construct(_type);
}

TCPSocket::~TCPSocket() {
  if (!closed) {
    close();
  }
}

// bool TCPSocket::bind(uint32_t port) {
//   return true;
//   //return ::bind(socket_descriptor, (struct sockaddr*)&address, sizeof(address)) > -1;
//   // errno = 0;
//   // ::bind(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
//   // if (errno != 0) {
//   //   printf("Bind: %s\n", strerror(errno));
//   // }

//   // return true;
// }

bool TCPSocket::connect(const char* ip, uint32_t port) {
  address.sin_addr.s_addr = inet_addr(ip);
  address.sin_port = htons(port);
  assert(address.sin_addr.s_addr != 0 && address.sin_port != 0);

  int32_t status = 0;
  if (connection_status == ConnectionStatus::Disconnected) {
    errno = 0;
    status = ::connect(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
    if (status == -1) {
      if (errno != 0) {
        switch (errno) {
          case EINPROGRESS:
          case EALREADY: {
            error_printf("Cannot stablish connection now; will keep trying\n");
            connection_status = ConnectionStatus::Connecting;

            break;
          }
          case EINVAL:
          case ECONNREFUSED: {
            close();
            construct(type);

            break;
          }
          default: {
            error_printf("Connect error not supported: %s\n", strerror(errno));
          }
        }
      }
    }
    else if (status == 0) {
      error_printf("CONNECTED\n");
      connection_status = ConnectionStatus::Connected;
    }
  }
  else if (connection_status == ConnectionStatus::Connecting) {
    fd_set sock_des;
    memset(&sock_des, 0, sizeof(sock_des));
    FD_ZERO(&sock_des);
    FD_SET(socket_descriptor, &sock_des);

    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    //timeout.tv_usec = 1;
    errno = 0;
    status = select(socket_descriptor + 1, nullptr, &sock_des, nullptr, &timeout);  // CAREFUL: need to ask for fds than can be WRITTEN, not READABLE
    if (status >= 0) {
      if (FD_ISSET(socket_descriptor, &sock_des) > 0) {
        int32_t error_state = 0;
        socklen_t sizeofint = sizeof(int32_t);
        int32_t result = getsockopt(socket_descriptor, SOL_SOCKET, SO_ERROR, &error_state, &sizeofint);
        if (result == 0) {
          if (error_state != 0) {
            error_printf("Query error value: %s\n", strerror(error_state));
            if (error_state == ECONNREFUSED) {
              close();
              construct(type);
            }
          }
          else {
            error_printf("No error, connected\n");
            connection_status = ConnectionStatus::Connected;
          }
        }
        else if (errno != 0) {
          error_printf("getsockopt: %s\n", strerror(errno));
        }
      }
    }
  }
  else {
    error_printf("Socket already connected, resetting...\n");
    close();
    construct(type);
  }

  return connection_status == ConnectionStatus::Connected;
}

uint32_t TCPSocket::sendData(byte* buffer, uint32_t buffer_size) {
  uint32_t bytes_sent = 0;
  int32_t status = 0;

  if (buffer_size == 0) {
    printf("ME CAGO EN LA WEA AGAIN\n");
  }

  if (sending_status == SendingStatus::CanSend) {
    errno = 0;
    status = ::send(socket_descriptor, buffer, buffer_size, 0);
    if (status == -1) {
      if (errno != 0) {
        switch (errno) {
          //case EAGAIN:  // looks like in MacOSX the following is defined: #define EWOULDBLOCK EAGAIN
          case EWOULDBLOCK: {
            sending_status = SendingStatus::Sending;

            break;
          }
          case EPIPE: {
            error_printf("The connection was closed\n");

            break;
          }
          case ECONNRESET: {
            // close();
            // construct(type);  // This sets connection_status to ConnectionStatus::Disconnected
            connection_status = ConnectionStatus::Disconnected;

            break;
          }
          default: {
            error_printf("Send data: %s\n", strerror(errno));

            break;
          }
        }
      }
    }
    else {
      // Data sent successfuly and status cannot be negative
      bytes_sent = (uint32_t)status;
    }
  }
  else if (sending_status == SendingStatus::Sending) {
    fd_set sock_des;
    memset(&sock_des, 0, sizeof(sock_des));
    FD_ZERO(&sock_des);
    FD_SET(socket_descriptor, &sock_des);

    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    //timeout.tv_usec = 1;
    errno = 0;

    status = select(socket_descriptor + 1, nullptr, &sock_des, nullptr, &timeout); // CAREFUL: test this
    if (status >= 0) {
      if (FD_ISSET(socket_descriptor, &sock_des) > 0) {
        int error_state = 0;
        socklen_t sizeofint = sizeof(int32_t);
        int result = getsockopt(socket_descriptor, SOL_SOCKET, SO_ERROR, &error_state, &sizeofint);
        if (result == 0) {
          if (error_state != 0) {
            error_printf("Query error value: %s\n", strerror(error_state));
          }
          else {
            errno = 0;
            status = ::send(socket_descriptor, buffer, buffer_size, 0);
            if (errno != 0) {
              error_printf("Send data: %s\n", strerror(errno));
            }
            if (status > 0) {
              bytes_sent = (uint32_t)status;
              sending_status = SendingStatus::CanSend;
            }
            else if (status == 0) {
              error_printf("Send returned 0 bytes\n");
            }
          }
        }
        else if (errno != 0) {
          error_printf("getsockopt: %s\n", strerror(errno));
        }
      }
      else {
        error_printf("Socket not ready to send\n");
      }
    }
    else {
      printf("Receive data select(): %s\n", strerror(errno));
    }
  }

  return bytes_sent;
}

uint32_t TCPSocket::receiveData(byte* buffer, uint32_t max_size_to_read) {
  uint32_t bytes_read = 0;
  int32_t status = 0;

  if (receiving_status == ReceivingStatus::CanReceive) {
    errno = 0;
    status = recv(socket_descriptor, buffer, max_size_to_read, 0);
    if (status == -1) {
      if (errno != 0) {
        switch (errno) {
          //case EAGAIN:  // looks like in MacOSX the following is defined: #define EWOULDBLOCK EAGAIN
          case EWOULDBLOCK: {
            receiving_status = ReceivingStatus::Receiving;

            break;
          }
          case ECONNRESET: {
            // close();
            // construct(type);
            connection_status = ConnectionStatus::Disconnected;

            break;
          }
          default: {
            error_printf("Receive data: %s\n", strerror(errno));
            //error_printf("%u\n", errno);

            break;
          }
        }
      }
    }
    else {
      bytes_read = (uint32_t)status;
    }
  }
  else if (receiving_status == ReceivingStatus::Receiving) {
    fd_set sock_des;
    memset(&sock_des, 0, sizeof(sock_des));
    FD_ZERO(&sock_des);
    FD_SET(socket_descriptor, &sock_des);

    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    //timeout.tv_usec = 1;
    errno = 0;
    status = select(socket_descriptor + 1, &sock_des, nullptr, nullptr, &timeout);  // CAREFUL: need to ask for fds than can be WRITTEN, not READABLE
    if (status >= 0) {
      if (FD_ISSET(socket_descriptor, &sock_des) > 0) {
        int32_t error_state = 0;
        socklen_t sizeofint = sizeof(int32_t);
        errno = 0;
        int32_t result = getsockopt(socket_descriptor, SOL_SOCKET, SO_ERROR, &error_state, &sizeofint);
        if (result == 0) {
          if (error_state != 0) {
            error_printf("Query error value: %s\n", strerror(error_state));
          }
          else {
            errno = 0;
            status = recv(socket_descriptor, buffer, max_size_to_read, 0);
            if (errno != 0) {
              //error_printf("Receive data: %s\n", strerror(errno));
              if (errno == EINVAL) {
                error_printf("Receive data: EINVAL\n");
              }
              else if (errno == EWOULDBLOCK) {
                error_printf("Receive data: EWOULDBLOCK\n");
              }
              else {
                error_printf("WEA\n");
              }
            }
            if (status >= 0) {
              bytes_read = (uint32_t)status;
              receiving_status = ReceivingStatus::CanReceive;
            }
          }
        }
        else if (errno != 0) {
          error_printf("getsockopt: %s\n", strerror(errno));
        }
      }
      else {
        error_printf("Socket not ready to receive\n");
      }
    }
    else {
      printf("Receive data select(): %s\n", strerror(errno));
    }
  }

  return bytes_read;
}

bool TCPSocket::close() {
  connection_status = ConnectionStatus::Disconnected;

  int32_t error_state = 0;
  socklen_t sizeofint = sizeof(int32_t);
  errno = 0;
  int32_t status = getsockopt(socket_descriptor, SOL_SOCKET, SO_ERROR, &error_state, &sizeofint);
  if (status == -1) {
    error_printf("getsockopt: %s\n", strerror(errno));
  }
  // at this point, any error in socket_descriptor should have been cleared
  errno = 0;
  shutdown(socket_descriptor, SHUT_RDWR);
  if (errno != 0) {
    error_printf("shutdown: %s\n", strerror(errno));
  }

  errno = 0;
  bool result = ::close(socket_descriptor) > -1;
  if (errno != 0) {
    error_printf("Close: %s\n", strerror(errno));
  }

  closed = result;

  return result;
}

bool TCPSocket::isConnected() const {
  return connection_status == ConnectionStatus::Connected;
}
  
/*private*/TCPSocket::TCPSocket(Type type, uint32_t descriptor) {
  socket_descriptor = descriptor;

  construct(type);
}

/*private*/TCPSocket::TCPSocket() {
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
                            // AF_INET
  socket_descriptor = socket(address.sin_family, SOCK_STREAM, 0);

  construct(Socket::Type::NonBlock);
}

/*private*/void TCPSocket::construct(Socket::Type _type) {
  type = _type;

  closed = false;

  connection_status = ConnectionStatus::Disconnected;
  receiving_status = ReceivingStatus::CanReceive;
  sending_status = SendingStatus::CanSend;

  int32_t true_int_value = 1;
  socklen_t sizeofunsignedint = sizeof(uint32_t);
  uint32_t max_recv_buffer_size = 0;
  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int32_t*)&true_int_value, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  
  getsockopt(socket_descriptor, SOL_SOCKET, SO_RCVBUF, (uint32_t*)&max_recv_buffer_size, &sizeofunsignedint);
  printf("Receive buffer size: %u\n", max_recv_buffer_size);
  //max_recv_buffer_size *= 4;
  setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVBUF, (uint32_t*)&max_recv_buffer_size, sizeofunsignedint);
  getsockopt(socket_descriptor, SOL_SOCKET, SO_RCVBUF, (uint32_t*)&max_recv_buffer_size, &sizeofunsignedint);
  printf("Receive buffer size: %u\n", max_recv_buffer_size);


  uint32_t max_send_buffer_size = 0;
  getsockopt(socket_descriptor, SOL_SOCKET, SO_SNDBUF, (uint32_t*)&max_send_buffer_size, &sizeofunsignedint);
  printf("Send buffer size: %u\n", max_send_buffer_size);
  //max_send_buffer_size *= 8;
  setsockopt(socket_descriptor, SOL_SOCKET, SO_SNDBUF, (uint32_t*)&max_send_buffer_size, sizeofunsignedint);
  getsockopt(socket_descriptor, SOL_SOCKET, SO_SNDBUF, (uint32_t*)&max_send_buffer_size, &sizeofunsignedint);
  printf("Send buffer size: %u\n", max_send_buffer_size);
  
  //setsockopt(socket_descriptor, IPPROTO_TCP, TCP_NODELAY, (int32_t*)&true_int_value, sizeof(int32_t));

  // TODO: give the option to SO_NOSIGPIPE to be flagable
  //setsockopt(socket_descriptor, SOL_SOCKET, SO_NOSIGPIPE, (int32_t*)&true_int_value, sizeof(int32_t));

  if (type == Type::NonBlock) {
    fcntl(socket_descriptor, F_SETFL, O_NONBLOCK);
  }
}

/*private*/int32_t TCPSocket::getDescriptor() const {
  return socket_descriptor;
}
// [\TCPSocket]



// [TCPListener]
TCPListener::TCPListener(Type _type, uint32_t _queue_size) {
  queue_size = _queue_size;
  construct(_type);
}

TCPListener::~TCPListener() {
  if (!closed) {
    close();
  }
}

bool TCPListener::bind(uint32_t port) {
  address.sin_addr.s_addr = htons(INADDR_ANY);
  address.sin_port = htons(port);

  return ::bind(socket_descriptor, (struct sockaddr*)&address, sizeof(address)) > -1;
}

bool TCPListener::listen() {
  listening_status = ListeningStatus::Listening;
  // TODO: should encapsule this into an if(not listening)
  return ::listen(socket_descriptor, queue_size) > -1;
}
  
TCPSocket* TCPListener::accept() {
  if (listening_status == ListeningStatus::Listening) {
    int32_t accepted_socket_des = ::accept(socket_descriptor, nullptr, nullptr);
    if (accepted_socket_des >= 0) {
      if (accepted_socket) {
        accepted_socket->close();
        delete accepted_socket;
      }
      accepted_socket = new TCPSocket(type, accepted_socket_des);
      accepted_socket->connection_status = TCPSocket::ConnectionStatus::Connected;
    }
    else {
      if (errno != 0) {
        error_printf("Accept: %s\n", strerror(errno));
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK) {  // looks like in MacOSX the following is defined: #define EWOULDBLOCK EAGAIN. But does ::accept() return both?
        error_printf("Will try to accept connection later...\n");
        listening_status = ListeningStatus::WaitingForAccept;
      }
    }
  }
  else if (listening_status == ListeningStatus::WaitingForAccept) {
    fd_set sock_des;
    FD_ZERO(&sock_des);
    FD_SET(socket_descriptor, &sock_des);

    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    errno = 0;
    int32_t status = select(socket_descriptor + 1, &sock_des, nullptr, nullptr, &timeout);
    //int status = select(socket_descriptor + 1, nullptr, &sock_des, nullptr, &timeout);
    //if (FD_ISSET(socket_descriptor, &sock_des) > 0) {
    if (status >= 0) {
      if (FD_ISSET(socket_descriptor, &sock_des) > 0) {
        int32_t error_state = 0;
        socklen_t sizeofint = sizeof(int32_t);
        int32_t result = getsockopt(socket_descriptor, SOL_SOCKET, SO_ERROR, &error_state, &sizeofint);
        if (result == 0) {
          if (error_state != 0) {
            error_printf("Query error value: %s\n", strerror(error_state));
          }
          else {
            error_printf("No error\n");
            int32_t accepted_socket_des = ::accept(socket_descriptor, nullptr, nullptr);
            if (accepted_socket_des >= 0 && accepted_socket) {
              accepted_socket->close();
              delete accepted_socket;
            }
            accepted_socket = new TCPSocket(type, accepted_socket_des);
            accepted_socket->connection_status = TCPSocket::ConnectionStatus::Connected;

            listening_status = ListeningStatus::Listening;
          }
        }
        if (errno != 0) {
          error_printf("getsockopt: %s\n", strerror(errno));
        }

        // int accepted_socket_des = ::accept(socket_descriptor, nullptr, nullptr);
        // if (accepted_socket_des >= 0 && accepted_socket) {
        //   accepted_socket->close();
        //   delete accepted_socket;
        // }
        // accepted_socket = new TCPSocket(type, accepted_socket_des);

        // listening_status = ListeningStatus::Listening;
      }
    }
    else if (errno != 0) {
      error_printf("Select: %s\n", strerror(errno));
    }
  }

  return accepted_socket;
}

// CAREFUL: can make the connected socket to be closed before the connected socket can finish doing its work
bool TCPListener::close() {
  bool success = true;

  if (accepted_socket) {
    success = shutdown(accepted_socket->getDescriptor(), SHUT_RDWR) > -1;
    delete accepted_socket;
  }
  
  int32_t error_state = 0;
  socklen_t sizeofint = sizeof(int32_t);
  errno = 0;
  int32_t status = getsockopt(socket_descriptor, SOL_SOCKET, SO_ERROR, &error_state, &sizeofint);
  if (status == -1) {
    error_printf("getsockopt: %s\n", strerror(errno));
  }
  // at this point, any error in socket_descriptor should have been cleared
  errno = 0;
  shutdown(socket_descriptor, SHUT_RDWR);
  if (errno != 0) {
    error_printf("shutdown: %s\n", strerror(errno));
  }

  errno = 0;
  success = (::close(socket_descriptor) > -1) | success;
  if (errno != 0) {
    error_printf("Close: %s\n", strerror(errno));
  }

  listening_status = ListeningStatus::NotListening;
  closed = success;

  return success;
}

/*private*/TCPListener::TCPListener() {
  construct(Socket::Type::NonBlock);
}

/*private*/void TCPListener::construct(Socket::Type _type) {
  type = _type;

  listening_status = ListeningStatus::NotListening;

  accepted_socket = nullptr;
  queue_size = 32;
  closed = false;

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;

  // Should put this in an init() method
  socket_descriptor = socket(address.sin_family, SOCK_STREAM, 0);
  int32_t true_int_value = 1;
  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int32_t*)&true_int_value, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  
  // TODO: give the option to SO_NOSIGPIPE to be flagable
  //setsockopt(socket_descriptor, SOL_SOCKET, SO_NOSIGPIPE, (int32_t*)&true_int_value, sizeof(int32_t));
  
  if (type == Type::NonBlock) {
    fcntl(socket_descriptor, F_SETFL, O_NONBLOCK);
  }
}
// [\TCPListener]
