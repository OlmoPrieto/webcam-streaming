#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>
#include <cassert>
#include <vector>

// v4l
#include <linux/videodev2.h>
#include <libv4l2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
// \v4l

// network
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
// \network

typedef uint8_t byte;

class Chrono {
public: 
  Chrono() {
  }
  ~Chrono() {
  }
  void start() {
    m_cStart = m_cClock.now();
  }
  void stop() {
    m_cEnd = m_cClock.now();
    m_cTime = std::chrono::duration_cast<std::chrono::duration<float>>(m_cEnd - m_cStart);
  }
  float timeAsSeconds() const {
    return m_cTime.count();
  }
  float timeAsMilliseconds() const {
    return m_cTime.count() * 1000.0f;
  }
  std::chrono::duration<float> m_cTime;
  std::chrono::high_resolution_clock m_cClock;
  std::chrono::high_resolution_clock::time_point m_cStart;
  std::chrono::high_resolution_clock::time_point m_cEnd;
};

class TCPListener {
public:
  enum class Type {
    NonBlock,
    Block,
  };

  TCPListener(Type type) {
    type = type;
    TCPListener();
  }

  ~TCPListener() {
    close();
  }

  bool bind(uint32_t port) {
    address.sin_addr.s_addr = htons(INADDR_ANY);
    address.sin_port = htons(port);

    return (bool)::bind(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
  }

  bool listen(uint32_t max_connections = 1) {
    return (bool)::listen(socket_descriptor, max_connections);
  }

  uint32_t waitForConnections() {
    uint32_t accepted_connections = 0;
    int accepted_socket_des = accept(socket_descriptor, nullptr, nullptr);
    while (accepted_socket_des >= 0) {
      accepted_sockets_desc.push_back(accepted_socket_des);

      accepted_socket_des = accept(socket_descriptor, nullptr, nullptr);
    }

    return accepted_connections;
  }

  // void sendData(byte* buffer, uint32_t buffer_size, uint32_t target_connection = 0) {
  //   ::send(accepted_sockets_desc[target_connection], buffer, buffer_size, 0);
  // }

  bool close() {
    bool success = true;
    for (uint32_t i = 0; i < accepted_sockets_desc.size(); ++i) {
      success = (bool)shutdown(accepted_sockets_desc[i], SHUT_RDWR);
    }

    // TODO: needed?
    success = ::close(socket_descriptor);

    return success;
  }

  bool closeConnection(uint32_t target_connection) {
    return (bool)shutdown(accepted_sockets_desc[target_connection], SHUT_RDWR);
  }

private:
  TCPListener() {
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;

    // Should put this in an init() method
    socket_descriptor = socket(address.sin_family, SOCK_STREAM, 0);
    setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int*)1, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
    int32_t flags = type == Type::NonBlock ? 0 : O_NONBLOCK;
    fcntl(socket_descriptor, F_SETFL, flags);
  }

  std::vector<int32_t> accepted_sockets_desc;
  struct sockaddr_in address;
  Type type;
  int32_t socket_descriptor;
};


class TCPSocket {
public:
  enum class Type {
    NonBlock,
    Block,
  };

  TCPSocket(Type type) {
    type = type;
    TCPSocket();
  }

  ~TCPSocket() {
    close();
  }

  bool bind(uint32_t port) {
    address.sin_port = htons(port);

    return (bool)::bind(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
  }

  bool connect(const char* ip) {
    address.sin_addr.s_addr = inet_addr(ip);
    assert(address.sin_addr.s_addr != 0 && address.sin_port != 0);
    return (bool)::connect(socket_descriptor, (struct sockaddr*)&address, sizeof(address));
  }

  void sendData(byte* buffer, uint32_t buffer_size) {
    ::send(socket_descriptor, buffer, buffer_size, 0);
  }

  bool close() {
    //return (bool)shutdown(socket_descriptor, SHUT_RDWR);
    return (bool)::close(socket_descriptor);
  }

private:
  TCPSocket() {
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;

    // Should put this in an init() method
    socket_descriptor = socket(address.sin_family, SOCK_STREAM, 0);
    setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (int*)1, sizeof(int32_t));  // CAREFUL: this violates TCP/IP protocol making it unlikely but possible for the next program that binds on that port to pick up packets intended for the original program
    int32_t flags = type == Type::NonBlock ? 0 : O_NONBLOCK;
    fcntl(socket_descriptor, F_SETFL, flags);
  }

  struct sockaddr_in address;
  Type type;
  int32_t socket_descriptor;
};


int main(int argc, char** argv) {

  TCPListener listener(TCPListener::Type::NonBlock);

  Chrono c;
  c.start();
  
  const char* device = (argc > 1) ? argv[1] : "/dev/video0";

  int fd = -1; // file_descriptor ?
  if ((fd = open(device, O_RDWR)) < 0) {
    perror("open");
    return 1;
  }

  struct v4l2_capability cap;
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
    perror("VIDIOC_QUERYCAP");
    return 1;
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    fprintf(stderr, "The device does not handle video capture\n");
    return 1;
  }

  struct v4l2_format format;
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  format.fmt.pix.width = 640;
  format.fmt.pix.height = 480;
  format.fmt.pix.colorspace = V4L2_COLORSPACE_JPEG;
  format.fmt.sdr.pixelformat = V4L2_PIX_FMT_JPEG;
  //format.fmt.sdr.buffersize = 640*480*3;

  if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
    perror("VIDIOC_S_FMT");
    return 1;
  }

  struct v4l2_streamparm fps;
  memset(&fps, 0, sizeof(fps));
  fps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(fd, VIDIOC_G_PARM, &fps) < 0) {
    perror("VIDIOC_G_PARM");
    return 1;
  }
  fps.parm.capture.timeperframe.numerator = 1;
  fps.parm.capture.timeperframe.denominator = 30;
  if (ioctl(fd, VIDIOC_S_PARM, &fps) < 0) {
    perror("VIDIOC_S_PARM");
    return 1; 
  }

  void* buffer = malloc(format.fmt.pix.sizeimage);
  //void* buffer = nullptr;
  //posix_memalign(&buffer, 16, format.fmt.pix.sizeimage);
  if (!buffer) {
    printf("Error requesting memory: malloc()\n");
  }

  struct v4l2_requestbuffers bufferrequest;
  memset(&bufferrequest, 0, sizeof(bufferrequest));
  bufferrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  //bufferrequest.memory = V4L2_MEMORY_MMAP; // TODO: check another methods
  bufferrequest.memory = V4L2_MEMORY_USERPTR;
  bufferrequest.count = 1;

  if (ioctl(fd, VIDIOC_REQBUFS, &bufferrequest) < 0) {
    perror("VIDIOC_REQBUFS");
    return 1;
  }

  struct v4l2_buffer bufferinfo;
  memset(&bufferinfo, 0, sizeof(bufferinfo));

  bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  //bufferinfo.memory = V4L2_MEMORY_MMAP;
  bufferinfo.memory = V4L2_MEMORY_USERPTR;
  bufferinfo.index = 0;
  bufferinfo.m.userptr = (unsigned long)buffer;
  bufferinfo.length = format.fmt.pix.sizeimage;

  // TODO: maybe it is needed to allocate a second buffer in the same way?

  // DAMN IIIIIIITTTTTT!!!!!
  // if (ioctl(fd, VIDIOC_QUERYBUF, &bufferinfo) < 0) {
  //   perror("VIDIOC_QUERYBUF");
  //   return 1;
  // }
  // NOTE: this call made USERPTR to malfunction, surely because it set some flag that made DQBUF to fail. GOD DAMN IT
  // DAMN IIIIIIITTTTTT!!!!!

  //void* buffer_start = mmap(NULL, bufferinfo.length, PROT_READ | PROT_WRITE,
  //  MAP_SHARED, fd, bufferinfo.m.offset);
  void* buffer_start = nullptr;

  if (buffer_start == MAP_FAILED) {
    //perror("mmap");
    //return 1;
  }

  //memset(buffer_start, 0, bufferinfo.length);

  c.stop();
  printf("Time to init: %.2fms\n", c.timeAsMilliseconds());

  //
  //struct v4l2_buffer bufferinfo2;
  //memset(&bufferinfo2, 0, sizeof(bufferinfo2));
   
  //bufferinfo2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  //bufferinfo2.memory = V4L2_MEMORY_USERPTR;
  //bufferinfo2.index = 0; /* Queueing buffer index 0. */
  
  c.start();
  if (ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0) {
    printf("Error: %s\n", strerror(errno));
    return 1;
  }
  c.stop();
  printf("Time to queue buffer: %.2fms\n", c.timeAsMilliseconds());

  c.start();
  // Activate streaming
  int type = bufferinfo.type;
  if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){
    perror("VIDIOC_STREAMON");
    printf("Error: %s\n", strerror(errno));
    exit(1);
  }
  c.stop();
  printf("Time to enable streaming: %.2fms\n", c.timeAsMilliseconds());
   
  /* MAIN LOOP */
  struct v4l2_buffer bufferinfo2;
  for (int i = 0; i < 50; ++i) {
    c.start();
    // memset(&bufferinfo, 0, sizeof(bufferinfo));
    // bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // //bufferinfo2.memory = V4L2_MEMORY_MMAP;
    // bufferinfo.memory = V4L2_MEMORY_USERPTR;
    // bufferinfo.index = 0; 
    // bufferinfo2.m.userptr = (unsigned long)buffer;
    // bufferinfo.length = format.fmt.pix.sizeimage;
    // The buffer's waiting in the outgoing queue.
    if(ioctl(fd, VIDIOC_DQBUF, &bufferinfo) < 0){
      perror("VIDIOC_DQBUF");
      exit(1);
    }

    if (ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0) {
      perror("VIDIOC_QBUF");
      exit(1);
    }
    c.stop();
    printf("Time to get frame: %.2fms\n", c.timeAsMilliseconds());
  }
  /* \MAIN LOOP */
   
  // Deactivate streaming
  if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0){
    perror("VIDIOC_STREAMOFF");
    exit(1);
  }

  printf("Image size: %d bytes\n", bufferinfo.length);

  // save to file
  int jpgfile = open("/home/pi/Desktop/image.jpeg", O_WRONLY | O_CREAT, 0660);
  if (jpgfile < 0) {
    perror("Open");
    return 1;
  }

  write(jpgfile, buffer, bufferinfo.length);
  close(jpgfile);
  // save to file

  close(fd);

  free(buffer);

  return 0;
}
