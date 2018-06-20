#include <atomic>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#ifdef __PLATFORM_LINUX__
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
#endif

#include "chrono.h"
#include "sockets.h"

typedef uint8_t byte;

enum class ProcessingState {
  NotProcessing = 0,
  Processing
};

enum class NetworkState {
  NoPeerConnected = 0,
  PeerConnected,
  Sending
};

// GLOBAL VARIABLES
#ifndef __PLATFORM_LINUX__
struct v4l2_buffer{};
struct v4l2_format{struct fmt{struct pix{int sizeimage;}pix;}fmt;};
#endif
struct v4l2_buffer g_bufferinfo;
struct v4l2_format g_format;
int32_t g_fd = -1;
uint32_t g_bytes_sent = 0;
bool g_program_should_finish = false;
std::atomic<bool> g_can_read_data_buffer;
std::atomic<bool> g_can_send_data;
std::atomic<bool> g_can_sync_processing;
std::atomic<bool> g_can_process_data;
std::atomic<bool> g_can_sync_network;
std::atomic<bool> g_can_start_processing;
std::atomic<bool> g_can_start_network;

byte** g_read_ptr      = nullptr;
byte** g_read_copy_ptr = nullptr;
byte** g_process_ptr   = nullptr;
byte** g_send_ptr      = nullptr;

Chrono g_chrono;
float elapsed_time = 0.0f;

ProcessingState g_processing_state = ProcessingState::NotProcessing;
NetworkState g_network_state = NetworkState::NoPeerConnected;


void InterruptSignalHandler(int32_t param) {
  g_program_should_finish = true;
}

#ifdef __PLATFORM_LINUX__
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
#endif

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

  //printf("Pixels that differ: %u\n", count);
  //printf("Time to process image: %.2fms\n", c.timeAsMilliseconds());
}

void ProcessingTask() {
  byte r = 0;
  byte g = 64;
  byte b = 128;

  while (!g_program_should_finish) {
    if (g_can_process_data == true) {

      switch (g_processing_state) {
        case ProcessingState::NotProcessing: {
          g_processing_state = ProcessingState::Processing;

          break;
        }
        case ProcessingState::Processing: {
          byte* ptr = *g_read_ptr;
          for (uint32_t i = 0; i < g_format.fmt.pix.sizeimage; i += 3) {
            *(ptr + 0) = r;
            *(ptr + 1) = g;
            *(ptr + 2) = b;

            ptr += 3;
          }
          r++; g++; b++;
          r %= 255; g %= 255; b %= 255;

          ProcessImage(*g_read_copy_ptr, g_format.fmt.pix.sizeimage, 
              *g_process_ptr, g_format.fmt.pix.sizeimage, nullptr, 0);

          g_processing_state = ProcessingState::NotProcessing;

          break;
        }
      }

      g_can_process_data = false;
      g_can_sync_processing = true;
    }
  }
}


void NetworkTask() {
  TCPListener listener(Socket::Type::NonBlock, 128);
  listener.bind(14194);
  listener.listen();

  TCPSocket* socket = nullptr;

  g_can_send_data = true;
  g_chrono.start();

  byte r = 0;
  byte g = 64;
  byte b = 128;

  bool success = false;
  while (!g_program_should_finish) {
    if (g_can_start_network == true) {
      g_can_start_network = false;

      g_chrono.stop();
      elapsed_time += g_chrono.timeAsMilliseconds();
      //printf("elapsed_time: %.2f\n", elapsed_time);
      if (elapsed_time >= 330.3333f) {
        //printf("Can send data\n");
        elapsed_time = 0.0f;
        g_can_send_data = true;
      }
      g_chrono.start();

      switch (g_network_state) {
        case NetworkState::NoPeerConnected: {
          while (!socket && !g_program_should_finish) {
            socket = listener.accept();
          }

          printf("Peer connected!\n");

          g_network_state = NetworkState::PeerConnected;

          break;
        }

        case NetworkState::PeerConnected: {
          if (!socket->isConnected()) {
            g_network_state = NetworkState::NoPeerConnected;
          }
          else if (g_can_send_data) {
            //printf("Sending...\n");
            g_network_state = NetworkState::Sending;
          }

          break;
        }

        case NetworkState::Sending: {
          // TODO: do this by combining rgb into a uint32_t and memsetting it
          // byte* ptr = *g_send_ptr;
          // for (uint32_t i = 0; i < g_format.fmt.pix.sizeimage; i += 3) {
          //   *(ptr + 0) = r;
          //   *(ptr + 1) = g;
          //   *(ptr + 2) = b;

          //   ptr += 3;
          // }
          // r++; g++; b++;
          // r %= 255; g %= 255; b %= 255;

          uint32_t bytes_sent = 0;
          g_bytes_sent = 0;
          if (g_can_send_data == true) {
            g_network_state = NetworkState::PeerConnected;

            while (g_bytes_sent < g_format.fmt.pix.sizeimage) {
              bytes_sent = socket->sendData((*g_send_ptr) + g_bytes_sent, g_format.fmt.pix.sizeimage - g_bytes_sent);

              g_bytes_sent += bytes_sent;

              if (g_program_should_finish == true) {
                break;
              }
            }

            printf("Sent %u bytes\n", g_bytes_sent);

            // The order is important
            g_can_send_data = false;
            g_can_sync_network = true;
          }

          break;
        }
      } // switch
    } // g_can_start_network == true
  }
}

int __main() {
	TCPListener listener(Socket::Type::NonBlock);
	listener.bind(14194);
	listener.listen();

	TCPSocket* socket = nullptr;
	while (socket == nullptr) {
		socket = listener.accept();
	}
	printf("Accepted connection\n");

	byte buffer[1000000];
	memset(buffer, 0, 1000000);
	buffer[1] = 14;

	uint32_t g_data_sent = 0;
  uint32_t data_sent = 0;

  for (int i = 0; i < 10; ++i) {
    g_data_sent = 0;
    data_sent = 0;
    while (g_data_sent < 1000000) {
      //printf("g_data_sent: %u\n", g_data_sent);
      data_sent = socket->sendData(buffer + g_data_sent, 1000000 - g_data_sent);
      if (data_sent > 0) {
        printf("Sent %u bytes\n", data_sent);
      }
      g_data_sent += data_sent;
    }

    printf("\n");
  }

  Chrono c;
  c.start();

  c.stop();
  printf("Time: %.2f ms\n", c.timeAsMilliseconds());
	printf("Data sent successfuly\n");

	return 0;
}

int main() {
  g_format.fmt.pix.sizeimage = 640 * 480 * 3;

  TCPListener listener(Socket::Type::NonBlock);
  listener.bind(14194);
  listener.listen();

  TCPSocket* socket = nullptr;
  while (socket == nullptr) {
    socket = listener.accept();
  }

  byte* send_ptr = (byte*)malloc(g_format.fmt.pix.sizeimage);

  uint32_t g_data_sent = 0;
  uint32_t r = 0, g = 128, b = 255;
  while (true) {
    byte* ptr = send_ptr;
    for (uint32_t i = 0; i < g_format.fmt.pix.sizeimage; i += 3) {
      *(ptr + 0) = r;
      *(ptr + 1) = g;
      *(ptr + 2) = b;

      ptr += 3;
    }
    r++; g++; b++;
    r %= 255; g %= 255; b %= 255;

    uint32_t data_sent = 0;
    g_data_sent = 0;
    while (g_data_sent < g_format.fmt.pix.sizeimage) {
      data_sent = socket->sendData(send_ptr + g_data_sent, g_format.fmt.pix.sizeimage - g_data_sent);
      if (data_sent > 0) {
        printf("Sent %u bytes\n", data_sent);
      }
      g_data_sent += data_sent;
    }
  }

  free(send_ptr);

  return true;
}

int ___main(int argc, char** argv) {
  signal(SIGINT, InterruptSignalHandler);

  g_can_sync_network = false;
  g_can_sync_processing = false;
  g_can_process_data = true;
  g_can_send_data = true;
  
  g_format.fmt.pix.sizeimage = 640 * 480 * 3;

  byte* read_ptr      = nullptr;
  byte* read_copy_ptr = nullptr;
  byte* process_ptr   = nullptr;
  byte* send_ptr      = nullptr;

  byte* buffers[4] = {
    (byte*)malloc(g_format.fmt.pix.sizeimage),
    (byte*)malloc(g_format.fmt.pix.sizeimage),
    (byte*)malloc(g_format.fmt.pix.sizeimage),
    (byte*)malloc(g_format.fmt.pix.sizeimage)
  };
  for (uint32_t i = 0; i < 4; ++i) {
    memset(buffers[i], 0, g_format.fmt.pix.sizeimage);
  }
  g_read_ptr      = &buffers[0];
  g_read_copy_ptr = &buffers[1];
  g_process_ptr   = &buffers[2];
  g_send_ptr      = &buffers[3];

  std::thread network_thread(NetworkTask);
  std::thread process_image_thread(ProcessingTask);

  const char* device = (argc > 1) ? argv[1] : "/dev/video0";
  //InitializeVideoDevice(device);

  #if defined(__PLATFORM_MACOSX__) || defined(__PLATFORM_WINDOWS__)
  // No video streaming
  #else
  EnableVideoStreaming();
  #endif


  /* MAIN LOOP */
  Chrono c;
  uint32_t index = 0;
  while (g_program_should_finish == false) {
    c.start();

    memcpy(*g_read_copy_ptr, *g_read_ptr, g_format.fmt.pix.sizeimage);
    g_can_start_processing = true;
    g_can_read_data_buffer = true;
    g_can_start_network = true;

    //GrabCameraFrame(read_ptr);

    // sync point
    if (g_can_sync_processing == true && g_can_sync_network == true) {
      printf("Swapping buffers...\n");

      g_read_ptr      = &buffers[(index) % 3];
      g_read_copy_ptr = &buffers[(index + 1) % 3];
      g_process_ptr   = &buffers[(index + 2) % 3];
      g_send_ptr      = &buffers[(index + 3) % 3];
      ++index;
      if (index > 1000000) {
        // to avoid overflows in long executions
        index = 0;
      }

      g_can_sync_processing = false;
      g_can_sync_network = false;
      g_can_process_data = true;
      g_can_send_data = true;
    }

    c.stop();
    //printf("Frame time: %.2fms\n", c.timeAsMilliseconds());
  }
  /* \MAIN LOOP */

  close(g_fd);

  network_thread.join();
  process_image_thread.join();

  //free(buffer);

  free(buffers[0]);
  free(buffers[1]);
  free(buffers[2]);
  free(buffers[3]);

  return 0;
}

