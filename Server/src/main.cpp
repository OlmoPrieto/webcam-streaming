#include <iostream>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdint>
#include <thread>
#include <vector>

// v4l
#include <linux/videodev2.h>
#include <libv4l2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
// \v4l

#include "chrono.h"
#include "sockets.h"

typedef uint8_t byte;

// GLOBAL VARIABLES
struct v4l2_buffer g_bufferinfo;
struct v4l2_format g_format;
int32_t g_fd = -1;
bool g_program_should_finish = false;

void InterruptSignalHandler(int32_t param) {
  g_program_should_finish = true;
}

void ProcessImage(byte* src_image, unsigned int src_size, 
  byte* dst_image, unsigned int dst_size, byte* out_data, unsigned int offset) {
  assert(src_size == dst_size && "src image size differs from dst image size");
  assert(offset < src_size && "offset out of bounds");

  Chrono c;
  c.start();

  unsigned int count = 0;
  byte* src_ptr = src_image;
  byte* dst_ptr = dst_image;
  for (unsigned int i = offset; i < src_size; ++i) {
    if (*src_ptr != *dst_ptr) {
      ++count;
    }
  }
  c.stop();

  printf("Pixels that differ: %u\n", count);
  printf("Time to process image: %.2fms\n", c.timeAsMilliseconds());
}

void InitializeNetwork(TCPListener* listener, TCPSocket* accepted_socket) {
  listener->bind(14194);
  listener->listen();

  while (!accepted_socket) {
    accepted_socket = listener->accept();
  }
}

void SendData(TCPSocket* socket, byte* buffer, uint32_t buffer_size) {
  Chrono c;
  c.start();

  bool result = false;
  while (!result) {
    result = socket->sendData(buffer, buffer_size);
  }

  c.stop();

  printf("Time to send image: %.2fms\n", c.timeAsMilliseconds());
}

void InitializeVideoDevice(const char* device_path) {
  Chrono c;
  c.start();

  int g_fd = -1;
  errno = 0;
  if ((g_fd = open(device_path, O_RDWR)) < 0) {
    printf("open: %s\n", strerror(errno));
  }

  struct v4l2_capability cap;
  memset(&cap, 0, sizeof(cap));
  errno = 0;
  if (ioctl(g_fd, VIDIOC_QUERYCAP, &cap) < 0) {
    printf("VIDIOC_QUERYCAP: %s\n", strerror(errno));
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    fprintf(stderr, "The device %s does not handle video capture\n", device_path);
  }

  // struct v4l2_format format;
  g_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  g_format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24; // CAREFUL: OpenGL likes 32 bits, not 24
  g_format.fmt.pix.width = 640;
  g_format.fmt.pix.height = 480;
  g_format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

  errno = 0;
  if (ioctl(g_fd, VIDIOC_S_FMT, &g_format) < 0) {
    printf("VIDIOC_S_FMT: %s\n", strerror(errno));
  }

  struct v4l2_streamparm fps;
  memset(&fps, 0, sizeof(fps));
  fps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  errno = 0;
  if (ioctl(g_fd, VIDIOC_G_PARM, &fps) < 0) {
    printf("VIDIOC_G_PARM: %s\n", strerror(errno));
  }
  fps.parm.capture.timeperframe.numerator = 1;
  fps.parm.capture.timeperframe.denominator = 30;
  errno = 0;
  if (ioctl(g_fd, VIDIOC_S_PARM, &fps) < 0) {
    printf("VIDIOC_S_PARM: %s\n", strerror(errno));
  }

  struct v4l2_ext_control control;
  control.id = V4L2_CID_EXPOSURE_AUTO;
  control.value = V4L2_EXPOSURE_MANUAL;
  errno = 0;
  if (ioctl(g_fd, VIDIOC_S_EXT_CTRLS, &control) < 0) {
    printf("Set auto exposure: %s\n", strerror(errno));
  }

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_FOCUS_AUTO;
  control.value = false;
  errno = 0;
  if (ioctl(g_fd, VIDIOC_S_EXT_CTRLS, &control) < 0) {
    printf("Setting auto focus: %s\n", strerror(errno));
  }

  // byte* buffer = (byte*)malloc(g_format.fmt.pix.sizeimage);
  // //void* buffer = nullptr;
  // //posix_memalign(&buffer, 16, g_format.fmt.pix.sizeimage);
  // if (!buffer) {
  //   printf("Error requesting memory: malloc()\n");
  // }

  struct v4l2_requestbuffers bufferrequest;
  memset(&bufferrequest, 0, sizeof(bufferrequest));
  bufferrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  bufferrequest.memory = V4L2_MEMORY_USERPTR;
  bufferrequest.count = 1;

  errno = 0;
  if (ioctl(g_fd, VIDIOC_REQBUFS, &bufferrequest) < 0) {
    printf("VIDIOC_REQBUFS: %s\n", strerror(errno));
  }

  // struct v4l2_buffer bufferinfo;
  memset(&g_bufferinfo, 0, sizeof(g_bufferinfo));

  g_bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  g_bufferinfo.memory = V4L2_MEMORY_USERPTR;
  g_bufferinfo.index = 0;
  //g_bufferinfo.m.userptr = (unsigned long)read_ptr;
  g_bufferinfo.length = g_format.fmt.pix.sizeimage;

  c.stop();
  printf("Time to init: %.2fms\n", c.timeAsMilliseconds());
}

void EnableVideoStreaming() {
  Chrono c;
  c.start();

  if (ioctl(g_fd, VIDIOC_QBUF, &g_bufferinfo) < 0) {
    printf("Error: %s\n", strerror(errno));
    close(g_fd);
    return;
  }
  c.stop();
  printf("Time to queue buffer: %.2fms\n", c.timeAsMilliseconds());

  c.start();
  // Activate streaming
  int32_t type = g_bufferinfo.type;
  errno = 0;
  if(ioctl(g_fd, VIDIOC_STREAMON, &type) < 0){
    printf("VIDIOC_STREAMON: %s\n", strerror(errno));
    close(g_fd);
    exit(1);
  }
  c.stop();
  printf("Time to enable streaming: %.2fms\n", c.timeAsMilliseconds());
}

void DisableVideoStreaming() {
  int32_t type = g_bufferinfo.type;
  errno = 0;
  // Deactivate streaming
  if(ioctl(g_fd, VIDIOC_STREAMOFF, &type) < 0){
    printf("VIDIOC_STREAMOFF: %s\n", strerror(errno));
    close(g_fd);
    exit(1);
  }
}

void GrabCameraFrame(byte* target_buffer) {
  Chrono c;

  c.start();

  g_bufferinfo.m.userptr = (unsigned long)target_buffer;

  errno = 0;
  // The buffer's waiting in the outgoing queue.
  if(ioctl(g_fd, VIDIOC_DQBUF, &g_bufferinfo) < 0){
    printf("VIDIOC_DQBUF: %s\n", strerror(errno));
    close(g_fd);
    exit(1);
  }

  errno = 0;
  if (ioctl(g_fd, VIDIOC_QBUF, &g_bufferinfo) < 0) {
    printf("VIDIOC_QBUF: %s\n", strerror(errno));
    close(g_fd);
    exit(1);
  }
  c.stop();
  printf("Time to get frame: %.2fms\n", c.timeAsMilliseconds());
}


int main(int argc, char** argv) {
  signal(SIGINT, InterruptSignalHandler);

  // TCPListener listener(Socket::Type::NonBlock);
  // listener.bind(14194);
  // listener.listen();
  
  const char* device = (argc > 1) ? argv[1] : "/dev/video0";
  InitializeVideoDevice(device);

  byte* buffers[4] = {
    (byte*)malloc(g_format.fmt.pix.sizeimage),
    (byte*)malloc(g_format.fmt.pix.sizeimage),
    (byte*)malloc(g_format.fmt.pix.sizeimage),
    (byte*)malloc(g_format.fmt.pix.sizeimage)
  };
  byte* read_ptr      = buffers[0];
  byte* read_copy_ptr = buffers[1];
  byte* process_ptr   = buffers[2];
  byte* send_ptr      = buffers[3];

  EnableVideoStreaming();

  TCPListener listener(Socket::Type::NonBlock, 128);
  TCPSocket* socket = nullptr;
  InitializeNetwork(&listener, socket);

  /* MAIN LOOP */
  uint32_t index = 0;
  while (g_program_should_finish == false) {
    memcpy(read_copy_ptr, read_ptr, g_format.fmt.pix.sizeimage);

    // std::thread(ProcessImage, read_ptr, g_format.fmt.pix.sizeimage, 
    //   process_ptr, g_format.fmt.pix.sizeimage, nullptr, 0);
    uint32_t image_size = g_format.fmt.pix.sizeimage;
    std::thread process_image_thread(
      [read_copy_ptr, image_size, process_ptr]() {
        ProcessImage(read_copy_ptr, image_size, 
          process_ptr, image_size, nullptr, 0);
      }
    );
    // std::thread(SendData, ...);
    std::thread send_data_thread(
      [socket, send_ptr, image_size]() {
        SendData(socket, send_ptr, image_size);
      }
    );

    GrabCameraFrame(read_ptr);

    // TODO: capture if Ctrl+C is called to wait for threads to finish
    // TODO: capture if Ctrl+C is called to wait for threads to finish
    // TODO: capture if Ctrl+C is called to wait for threads to finish
    // TODO: capture if Ctrl+C is called to wait for threads to finish

    // sync point
    process_image_thread.join();
    send_data_thread.join();

    read_ptr      = buffers[(index) % 3];
    read_copy_ptr = buffers[(index + 1) % 3];
    process_ptr   = buffers[(index + 2) % 3];
    send_ptr      = buffers[(index + 3) % 3];
    ++index;
    if (index > 1000000) {
      index = 0;
    }
  }
  /* \MAIN LOOP */
   
  
  // printf("Image size: %d bytes\n", bufferinfo.length);
  // // save to file
  // int jpgfile = open("/home/pi/Desktop/image.jpeg", O_WRONLY | O_CREAT, 0660);
  // if (jpgfile < 0) {
  //   perror("Open");
  //   return 1;
  // }

  // write(jpgfile, buffer, bufferinfo.length);
  // close(jpgfile);
  // // save to file

  close(g_fd);

  //free(buffer);

  free(buffers[0]);
  free(buffers[1]);
  free(buffers[2]);
  free(buffers[3]);

  return 0;
}

