#include "sockets.h"

#include <cassert>
#include <cerrno>

#define IGNORE_PRINTF 1
#if IGNORE_PRINTF == 1
  #define printf(fmt, ...) (0)
#endif

// [TCPSocket]
TCPSocket::TCPSocket(Type _type) {
  type = _type;
  construct();
}

TCPSocket::~TCPSocket() {
  close();
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
          case EALREADY:
          case EINTR: {
            printf("Cannot stablish connection now; will keep trying\n");
            connection_status = ConnectionStatus::Connecting;

            break;
          }
          case EINVAL:
          case ECONNREFUSED: {
            close();
            construct();

            break;
          }
          default: {
            printf("Connect error not supported: %s\n", strerror(errno));
          }
        }
      }
    }
    else if (status == 0) {
      printf("CONNECTED\n");
      connection_status = ConnectionStatus::Connected;
    }
  }
  else if (connection_status == ConnectionStatus::Connecting) {
    struct fd_set sock_des;
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
            printf("Query error value: %s\n", strerror(error_state));
            if (error_state == ECONNREFUSED) {
              close();
              construct();
            }
          }
          else {
            printf("No error\n");
            connection_status = ConnectionStatus::Connected;
          }
        }
        else if (errno != 0) {
          printf("getsockopt: %s\n", strerror(errno));
        }
      }
    }
  }
  else {
    printf("Socket already connected\n");
  }

  return connection_status == ConnectionStatus::Connected;
}

bool TCPSocket::sendData(byte* buffer, uint32_t buffer_size) {
  uint32_t bytes_sent = 0;
  int32_t status = 0;

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
            printf("The connection was closed\n");

            break;
          }
          default: {
            printf("Send data: %s\n", strerror(errno));

            break;
          }
        }
      }
    }
  }
  else if (sending_status == SendingStatus::Sending) {
    struct fd_set sock_des;
    memset(&sock_des, 0, sizeof(sock_des));
    FD_ZERO(&sock_des);
    FD_SET(socket_descriptor, &sock_des);

    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    //timeout.tv_usec = 1;
    errno = 0;
    status = select(socket_descriptor + 1, &sock_des, nullptr, nullptr, &timeout); // CAREFUL: test this
    if (status >= 0) {
      if (FD_ISSET(socket_descriptor, &sock_des) > 0) {
        int error_state = 0;
        socklen_t sizeofint = sizeof(int32_t);
        int result = getsockopt(socket_descriptor, SOL_SOCKET, SO_ERROR, &error_state, &sizeofint);
        if (result == 0) {
          if (error_state != 0) {
            printf("Query error value: %s\n", strerror(error_state));
          }
          else {
            printf("No error\n");
            errno = 0;
            status = ::send(socket_descriptor, buffer, buffer_size, 0);
            if (errno != 0) {
              printf("Send data: %s\n", strerror(errno));
            }
            if (status >= 0) {
              bytes_sent = (uint32_t)status;
              sending_status = SendingStatus::CanSend;
            }
          }
        }
        else if (errno != 0) {
          printf("getsockopt: %s\n", strerror(errno));
        }
      }
    }
  }

  return bytes_sent > 0;
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
          default: {
            printf("Receive data: %s\n", strerror(errno));

            break;
          }
        }
      }
    }

    if (status >= 0) {
      bytes_read = (uint32_t)status;
    }
  }
  else if (receiving_status == ReceivingStatus::Receiving) {
    struct fd_set sock_des;
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
            printf("Query error value: %s\n", strerror(error_state));
          }
          else {
            printf("No error\n");
            errno = 0;
            status = recv(socket_descriptor, buffer, max_size_to_read, 0);
            if (errno != 0) {
              printf("Receive data: %s\n", strerror(errno));
            }
            if (status >= 0) {
              bytes_read = (uint32_t)status;
              receiving_status = ReceivingStatus::CanReceive;
            }
          }
        }
        else if (errno != 0) {
          printf("getsockopt: %s\n", strerror(errno));
        }
      }
    }
  }

  return bytes_read;
}

bool TCPSocket::close() {
  connection_status = ConnectionStatus::Disconnected;
  errno = 0;
  bool result = ::close(socket_descriptor) > -1;
  if (errno != 0) {
    printf("Close: %s\n", strerror(errno));
  }

  return result;
}
  
/*private*/TCPSocket::TCPSocket(Type type, uint32_t descriptor) {
  connection_status = ConnectionStatus::Disconnected;
  receiving_status = ReceivingStatus::CanReceive;
  sending_status = SendingStatus::CanSend;

  socket_descriptor = descriptor;

  int32_t true_int_value = 1;
  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int32_t*)&true_int_value, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  
  // TODO: give the option to SO_NOSIGPIPE to be flagable
  setsockopt(socket_descriptor, SOL_SOCKET, SO_NOSIGPIPE, (int32_t*)&true_int_value, sizeof(int32_t));

  if (type == Type::NonBlock) {
    fcntl(socket_descriptor, F_SETFL, O_NONBLOCK);
  }
}

/*private*/TCPSocket::TCPSocket() {
  construct();
}

/*private*/void TCPSocket::construct() {
  connection_status = ConnectionStatus::Disconnected;
  receiving_status = ReceivingStatus::CanReceive;
  sending_status = SendingStatus::CanSend;

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;

  socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
  int32_t true_int_value = 1;
  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int32_t*)&true_int_value, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  
  // TODO: give the option to SO_NOSIGPIPE to be flagable
  setsockopt(socket_descriptor, SOL_SOCKET, SO_NOSIGPIPE, (int32_t*)&true_int_value, sizeof(int32_t));

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
  type = _type;
  queue_size = _queue_size;
  construct();
}

TCPListener::~TCPListener() {
  close();
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

      //listening_status = ListeningStatus::Listening;
    }
    else {
      if (errno != 0) {
        printf("Accept: %s\n", strerror(errno));
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK) {  // looks like in MacOSX the following is defined: #define EWOULDBLOCK EAGAIN. But does ::accept() return both?
        printf("Will try to accept connection later...\n");
        listening_status = ListeningStatus::WaitingForAccept;
      }
    }
  }
  else if (listening_status == ListeningStatus::WaitingForAccept) {
    struct fd_set sock_des;
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
            printf("Query error value: %s\n", strerror(error_state));
          }
          else {
            printf("No error\n");
            int32_t accepted_socket_des = ::accept(socket_descriptor, nullptr, nullptr);
            if (accepted_socket_des >= 0 && accepted_socket) {
              accepted_socket->close();
              delete accepted_socket;
            }
            accepted_socket = new TCPSocket(type, accepted_socket_des);

            listening_status = ListeningStatus::Listening;
          }
        }
        if (errno != 0) {
          printf("getsockopt: %s\n", strerror(errno));
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
      printf("Select: %s\n", strerror(errno));
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
  
  success = (::close(socket_descriptor) > -1) | success;

  listening_status = ListeningStatus::NotListening;

  return success;
}

/*private*/TCPListener::TCPListener() {
  construct();
}

/*private*/void TCPListener::construct() {
  listening_status = ListeningStatus::NotListening;

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;

  // Should put this in an init() method
  socket_descriptor = socket(address.sin_family, SOCK_STREAM, 0);
  int32_t true_int_value = 1;
  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int32_t*)&true_int_value, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  
  // TODO: give the option to SO_NOSIGPIPE to be flagable
  setsockopt(socket_descriptor, SOL_SOCKET, SO_NOSIGPIPE, (int32_t*)&true_int_value, sizeof(int32_t));
  
  if (type == Type::NonBlock) {
    fcntl(socket_descriptor, F_SETFL, O_NONBLOCK);
  }
}
// [\TCPListener]