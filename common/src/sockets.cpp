#include "sockets.h"

#include <cassert>
#include <cerrno>

// [TCPSocket]
TCPSocket::TCPSocket(Type _type) {
  type = _type;
  construct();
}

TCPSocket::~TCPSocket() {
  close();
}

bool TCPSocket::bind(uint32_t port) {
  return true;
  //return ::bind(socket_descriptor, (struct sockaddr*)&address, sizeof(address)) > -1;
  // errno = 0;
  // ::bind(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
  // if (errno != 0) {
  //   printf("Bind: %s\n", strerror(errno));
  // }

  // return true;
}

bool TCPSocket::connect(const char* ip, uint32_t port) {
  struct fd_set sock_des;
  memset(&sock_des, 0, sizeof(sock_des));
  FD_ZERO(&sock_des);
  FD_SET(socket_descriptor, &sock_des);

  address.sin_addr.s_addr = inet_addr(ip);
  address.sin_port = htons(port);
  assert(address.sin_addr.s_addr != 0 && address.sin_port != 0);
  int status = 0;
  if (connection_status == ConnectionStatus::Disconnected) {
    errno = 0;
    status = ::connect(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
    if (errno != 0) {
      printf("Connect: %s\n", strerror(errno));
    }
    if (status == -1) {
      //printf("%s\n", strerror(errno));
      if (errno == EINPROGRESS/* || errno == EALREADY*/) {
        printf("Cannot stablish connection now; will keep trying\n");
        connection_status = ConnectionStatus::Connecting;
      }
      else if (errno == ECONNREFUSED) {
        uint32_t port = ntohs(address.sin_port);
        close();
        construct();
        bind(port);
      }
      else if (errno == EINVAL) {
        //printf("Invalid argument\n");
        uint32_t port = ntohs(address.sin_port);
        close();
        construct();
        bind(port);
      }
      else if (errno == EINTR) {
        printf("WTF bro\n");
      }
    }
    else if (status == 0) {
      printf("CONNECTED\n");
      connection_status = ConnectionStatus::Connected;
    }
  }
  else if (connection_status == ConnectionStatus::Connecting) {
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    //timeout.tv_usec = 1;
    errno = 0;
    status = select(socket_descriptor + 1, &sock_des, nullptr, nullptr, &timeout);
    if (status >= 0) {
      if (FD_ISSET(socket_descriptor, &sock_des) > 0) {
        int error_state = 0;
        socklen_t sizeofint = sizeof(int);
        int result = getsockopt(socket_descriptor, SOL_SOCKET, SO_ERROR, &error_state, &sizeofint);
        if (result == 0) {
          if (error_state != 0) {
            printf("Query error value: %s\n", strerror(error_state));
          }
          else {
            printf("NAISU\n");
            connection_status = ConnectionStatus::Connected;
          }
        }
        if (errno != 0) {
          printf("getsockopt: %s\n", strerror(errno));
        }

        // status = ::connect(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
        // if (status == -1) {
        //   if (errno == EINPROGRESS/* || errno == EALREADY*/) {
        //     printf("Cannot stablish connection now; will keep trying\n");
        //     connection_status = ConnectionStatus::Connecting;
        //   }
        //   else if (errno == EINVAL) {
        //     //printf("Invalid argument\n");
        //     uint32_t port = ntohs(address.sin_port);
        //     close();
        //     construct();
        //     bind(port);
        //   }
        //   else if (errno == EINTR) {
        //     printf("WTF bro\n");
        //   }
        // }
        // else if (status == 0) {
        //   printf("IS A MIRACLE\n");
        //   connection_status = ConnectionStatus::Connected;
        // }
      }
    }
  }
  // else if (connection_status == ConnectionStatus::Connected) {
  //   if (FD_ISSET(socket_descriptor, &sock_des)) {
  //     connection_status = ConnectionStatus::Connected;
  //   }
  // }
  else {
    printf("Socket already connected\n");
  }

  return connection_status == ConnectionStatus::Connected;
}

void TCPSocket::sendData(byte* buffer, uint32_t buffer_size) {
  ::send(socket_descriptor, buffer, buffer_size, 0);
}

uint32_t TCPSocket::receiveData(byte* buffer, uint32_t max_size_to_read) {
  return recv(socket_descriptor, buffer, max_size_to_read, 0);
}

bool TCPSocket::close() {
  connection_status = ConnectionStatus::Disconnected;

  return ::close(socket_descriptor) != -1;
}
  
/*private*/TCPSocket::TCPSocket(Type type, uint32_t descriptor) {
  socket_descriptor = descriptor;

  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int*)1, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  // int32_t flags = type == Type::NonBlock ? 0 : O_NONBLOCK;
  // fcntl(socket_descriptor, F_SETFL, flags);
  if (type == Type::NonBlock) {
    fcntl(socket_descriptor, F_SETFL, O_NONBLOCK);
  }
}

/*private*/TCPSocket::TCPSocket() {
  construct();
}

/*private*/void TCPSocket::construct() {
  connection_status = ConnectionStatus::Disconnected;

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;

  socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
  int32_t yass = 1;
  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int32_t*)&yass, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  // int32_t flags = type == Type::NonBlock ? O_NONBLOCK : 0;
  // fcntl(socket_descriptor, F_SETFL, flags);
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
  // struct sockaddr_in address;
  // memset(&address, 0, sizeof(sockaddr_in));
  // uint32_t size = sizeof(struct sockaddr_in);
  // int accepted_socket_des = ::accept(socket_descriptor, (struct sockaddr*)(&address), &size);

  // int accepted_socket_des = ::accept(socket_descriptor, nullptr, nullptr);
  // if (accepted_socket_des >= 0) {
  //   if (accepted_socket) {
  //     accepted_socket->close();
  //     delete accepted_socket;
  //   }
  //   accepted_socket = new TCPSocket(type, accepted_socket_des);
  // }

  if (listening_status == ListeningStatus::Listening) {
    int accepted_socket_des = ::accept(socket_descriptor, nullptr, nullptr);
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

      if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
    int status = select(socket_descriptor + 1, &sock_des, nullptr, nullptr, &timeout);
    //if (FD_ISSET(socket_descriptor, &sock_des) > 0) {
    if (status >= 0) {
      if (FD_ISSET(socket_descriptor, &sock_des) > 0) {
        int error_state = 0;
        socklen_t sizeofint = sizeof(int);
        int result = getsockopt(socket_descriptor, SOL_SOCKET, SO_ERROR, &error_state, &sizeofint);
        if (result == 0) {
          printf("Query error value: %s\n", strerror(error_state));
        }
        if (errno != 0) {
          printf("getsockopt: %s\n", strerror(errno));
        }

        int accepted_socket_des = ::accept(socket_descriptor, nullptr, nullptr);
        if (accepted_socket_des >= 0 && accepted_socket) {
          accepted_socket->close();
          delete accepted_socket;
        }
        accepted_socket = new TCPSocket(type, accepted_socket_des);

        listening_status = ListeningStatus::Listening;
      }
    }
    else if (errno != 0) {
      printf("Select: %s\n", strerror(errno));
    }
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
  int32_t yass = 1;
  setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int32_t*)&yass, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
  // int32_t flags = type == Type::NonBlock ? O_NONBLOCK : 0;
  // fcntl(socket_descriptor, F_SETFL, flags);
  if (type == Type::NonBlock) {
    fcntl(socket_descriptor, F_SETFL, O_NONBLOCK);
  }
}
// [\TCPListener]