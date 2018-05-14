#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>

#include "chrono.h"
#include "sockets.h"

#ifdef __PLATFORM_MACOSX__
  #include <OpenGL/gl3.h>
#endif
#ifdef __PLATFORM_LINUX__
  #include <glew/include/GL/glew.h>
#endif
#include <GLFW/include/glfw3.h>

typedef unsigned char byte;

enum class NetworkState {
  NotConnected = 0,
  Connected = 1,
  Receiving = 2
};

// GLOBAL VARIABLES
GLFWwindow* g_window = nullptr;
//byte* g_data = nullptr;
uint32_t g_window_width = 640;
uint32_t g_window_height = 480;
uint32_t g_image_width = 640;
uint32_t g_image_height = 480;
bool g_program_should_finish = false;

GLuint g_vao_id = 0;
GLuint g_vertex_buffer_id = 0;
GLuint g_other_vertex_shader_id = 0;
GLuint g_vertex_shader_id = 0;
GLuint g_fragment_shader_id = 0;
GLuint g_program_id = 0;
GLuint g_uvs_id = 0;
GLuint g_texture_id = 0;

byte* g_recv_data_ptr = nullptr;
byte* g_recv_data_buffer = nullptr;
byte* g_draw_buffer_ptr = nullptr;
byte* g_draw_buffer = nullptr;

TCPSocket g_socket(Socket::Type::NonBlock);
NetworkState g_network_state = NetworkState::NotConnected;
std::atomic<bool> g_can_sync_network;


class Mat4 {
public:
  Mat4() {
    setIdentity();
  }

  ~Mat4() {

  }

  void setIdentity() {
    memset(matrix, 0, sizeof(float) * 16);
    matrix[0]  = 1.0f;
    matrix[5]  = 1.0f;
    matrix[10] = 1.0f;
    matrix[15] = 1.0f;
  }

  void operator =(const Mat4& other) {
    matrix[0]  = other.matrix[0];  matrix[1]  = other.matrix[1];  matrix[2]  = other.matrix[2];  matrix[3]  = other.matrix[3];
    matrix[4]  = other.matrix[4];  matrix[5]  = other.matrix[5];  matrix[6]  = other.matrix[6];  matrix[7]  = other.matrix[7];
    matrix[8]  = other.matrix[8];  matrix[9]  = other.matrix[9];  matrix[10] = other.matrix[10]; matrix[11] = other.matrix[11];
    matrix[12] = other.matrix[12]; matrix[13] = other.matrix[13]; matrix[14] = other.matrix[14]; matrix[15] = other.matrix[15];
  }

  Mat4 operator *(const Mat4& other) {
    Mat4 result;
    result.matrix[0]  = matrix[0] * other.matrix[0] + matrix[1] * other.matrix[4] + matrix[2] * other.matrix[8]  + matrix[3] * other.matrix[12];
    result.matrix[1]  = matrix[0] * other.matrix[1] + matrix[1] * other.matrix[5] + matrix[2] * other.matrix[9]  + matrix[3] * other.matrix[13];
    result.matrix[2]  = matrix[0] * other.matrix[2] + matrix[1] * other.matrix[6] + matrix[2] * other.matrix[10] + matrix[3] * other.matrix[14];
    result.matrix[3]  = matrix[0] * other.matrix[3] + matrix[1] * other.matrix[7] + matrix[2] * other.matrix[11] + matrix[3] * other.matrix[15];
  
    result.matrix[4]  = matrix[4] * other.matrix[0] + matrix[5] * other.matrix[4] + matrix[6] * other.matrix[8]  + matrix[7] * other.matrix[12];
    result.matrix[5]  = matrix[4] * other.matrix[1] + matrix[5] * other.matrix[5] + matrix[6] * other.matrix[9]  + matrix[7] * other.matrix[13];
    result.matrix[6]  = matrix[4] * other.matrix[2] + matrix[5] * other.matrix[6] + matrix[6] * other.matrix[10] + matrix[7] * other.matrix[14];
    result.matrix[7]  = matrix[4] * other.matrix[3] + matrix[5] * other.matrix[7] + matrix[6] * other.matrix[11] + matrix[7] * other.matrix[15];
  
    result.matrix[8]  = matrix[8] * other.matrix[0] + matrix[9] * other.matrix[4] + matrix[10] * other.matrix[8]  + matrix[11] * other.matrix[12];
    result.matrix[9]  = matrix[8] * other.matrix[1] + matrix[9] * other.matrix[5] + matrix[10] * other.matrix[9]  + matrix[11] * other.matrix[13];
    result.matrix[10] = matrix[8] * other.matrix[2] + matrix[9] * other.matrix[6] + matrix[10] * other.matrix[10] + matrix[11] * other.matrix[14];
    result.matrix[11] = matrix[8] * other.matrix[3] + matrix[9] * other.matrix[7] + matrix[10] * other.matrix[11] + matrix[11] * other.matrix[15];
  
    result.matrix[12] = matrix[12] * other.matrix[0] + matrix[13] * other.matrix[4] + matrix[14] * other.matrix[8]  + matrix[15] * other.matrix[12];
    result.matrix[13] = matrix[12] * other.matrix[1] + matrix[13] * other.matrix[5] + matrix[14] * other.matrix[9]  + matrix[15] * other.matrix[13];
    result.matrix[14] = matrix[12] * other.matrix[2] + matrix[13] * other.matrix[6] + matrix[14] * other.matrix[10] + matrix[15] * other.matrix[14];
    result.matrix[15] = matrix[12] * other.matrix[3] + matrix[13] * other.matrix[7] + matrix[14] * other.matrix[11] + matrix[15] * other.matrix[15];
  
    return result;
  }

  void operator *=(const Mat4& other) {
    *this = *this * other;
  }


  float matrix[16];
};

static float fov = 60.0f;
static float near = 0.1f;
static float far = 1000.0f;

void InterruptSignalHandler(int32_t param) {
  glfwSetWindowShouldClose(g_window, true);
  g_program_should_finish = true;
}

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
    g_program_should_finish = true;
  }
}

static const char* vertex_shader_text = 
"#version 330 core\n"
"uniform mat4 MVP;\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec2 uv;\n"
"out vec2 o_uv;\n"
"void main() {\n"
"  gl_Position = MVP * vec4(position, 1.0);\n"
"  o_uv = uv;\n"
"}\n";

static const char* fragment_shader_text = 
"#version 330 core\n"
"uniform sampler2D target_texture;\n"
"in vec2 o_uv;\n"
"out vec4 color;\n"
"void main() {\n"
"  //color = vec4(o_uv.x, o_uv.y, 0.0, 1.0);\n"
"  color = vec4(texture(target_texture, o_uv).rgb, 1.0);\n"
"}\n";

bool CheckGLError(const char* tag = "") {
  GLenum error = glGetError();
  switch(error) {
    case GL_INVALID_OPERATION: {
      printf("%s : Invalid operation\n", tag);
      break;
    }
    case GL_INVALID_VALUE: {
      printf("%s : Invalid value\n", tag);
      break;
    }
    case GL_INVALID_ENUM: {
      printf("%s : Invalid enum\n", tag);
      break;
    }
    case GL_STACK_OVERFLOW: {
      printf("%s : Stack overflow\n", tag);
      break;
    }
    case GL_STACK_UNDERFLOW: {
      printf("%s : Stack underflow\n", tag);
      break;
    }
    case GL_OUT_OF_MEMORY: {
      printf("%s : Out of memory\n", tag);
      break;
    }
    // case GL_INVALID_FRAMEBUFFER_OPERATION: {
    //   printf("Invalid framebuffer operation: %s\n", tag);
    //   break;
    // }
  }

  return error != GL_NO_ERROR;
}

void InitializeGraphics() {
  printf("Initializing graphic context...\n");

  if (!glfwInit()) {
    printf("Failed to initialize GLFW\n");
    exit(1);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  #ifdef __PLATFORM_MACOSX__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  #endif

  g_window = glfwCreateWindow(g_window_width, g_window_height, "Window", NULL, NULL);
  if (!g_window) {
    glfwTerminate();
    printf("Failed to create window\n");
    exit(1);
  }

  glfwMakeContextCurrent(g_window);
  //glfwSwapInterval(0); // comment/uncomment to enable/disable 'vsync'
  glfwSetKeyCallback(g_window, KeyCallback);

  #ifdef __PLATFORM_LINUX__
    glewInit();
  #endif
}

void InitializeOpenGLStuff() {
  printf("Initializing OpenGL...\n");

  GLenum error = GL_NO_ERROR;
  printf("GL_NO_ERROR code: %d\n", error);

  //GLuint vao;
  glGenVertexArrays(1, &g_vao_id);
  glBindVertexArray(g_vao_id);
  
  //GLuint vertex_buffer_id;
  struct Vertex {
    float x, y, z;
  };
  Vertex vertices[6] = {
    {  0.9f,  0.9f, 0.0f },
    { -0.9f,  0.9f, 0.0f },
    { -0.9f, -0.9f, 0.0f },
    { -0.9f, -0.9f, 0.0f },
    {  0.9f, -0.9f, 0.0f },
    {  0.9f,  0.9f, 0.0f }
  };
  glGenBuffers(1, &g_vertex_buffer_id);
  glBindBuffer(GL_ARRAY_BUFFER, g_vertex_buffer_id);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  GLuint g_other_vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
  CheckGLError("glCreateShader vertex 2");

  g_vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
  CheckGLError("glCreateShader vertex");
  glShaderSource(g_vertex_shader_id, 1, &vertex_shader_text, NULL);
  CheckGLError("glShaderSource vertex");
  glCompileShader(g_vertex_shader_id);
  CheckGLError("glCompileShader vertex");
  GLint vertex_shader_compiling_success = 0;
  glGetShaderiv(g_vertex_shader_id, GL_COMPILE_STATUS, &vertex_shader_compiling_success);
  if (!vertex_shader_compiling_success) {
    printf("Failed to compile vertex shader\n");
    GLint log_size = 0;
    glGetShaderiv(g_vertex_shader_id, GL_INFO_LOG_LENGTH, &log_size);
    char* log = (char*)malloc(log_size);
    GLint read = 0;
    glGetShaderInfoLog(g_vertex_shader_id, log_size, &read, log);
    printf("Error: %s\n", log);
    free(log);
  }

  g_fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(g_fragment_shader_id, 1, &fragment_shader_text, NULL);
  glCompileShader(g_fragment_shader_id);
  CheckGLError("glCompileShader fragment");
  GLint fragment_shader_compiling_success = 0;
  glGetShaderiv(g_fragment_shader_id, GL_COMPILE_STATUS, &fragment_shader_compiling_success);
  if (!fragment_shader_compiling_success) {
    printf("Failed to compile fragment shader\n");
    GLint log_size = 0;
    glGetShaderiv(g_fragment_shader_id, GL_INFO_LOG_LENGTH, &log_size);
    char* log = (char*)malloc(log_size);
    GLint read = 0;
    glGetShaderInfoLog(g_fragment_shader_id, log_size, &read, log);
    printf("Error: %s\n", log);
    free(log);
  }

  GLuint g_program_id = glCreateProgram();
  glAttachShader(g_program_id, g_vertex_shader_id);
  glAttachShader(g_program_id, g_fragment_shader_id);
  glLinkProgram(g_program_id);
  CheckGLError("glLinkProgram");

  GLint mvp_location = glGetUniformLocation(g_program_id, "MVP");
  GLint position_location = glGetAttribLocation(g_program_id, "position");

  glBindBuffer(GL_ARRAY_BUFFER, g_vertex_buffer_id);
  glEnableVertexAttribArray(position_location);
  CheckGLError("glEnableVertexAttribArray 1");
  glVertexAttribPointer(position_location, 3, GL_FLOAT, GL_FALSE, 0, 0);
  CheckGLError("glVertexAttribPointer 1");

  //GLuint g_uvs_id;
  struct UV {
    float u, v;
  };
  UV uvs[6] = {
    {  1.0f,  1.0f },
    { -1.0f,  1.0f },
    { -1.0f, -1.0f },
    { -1.0f, -1.0f },
    {  1.0f, -1.0f },
    {  1.0f,  1.0f }
  };
  glGenBuffers(1, &g_uvs_id);
  glBindBuffer(GL_ARRAY_BUFFER, g_uvs_id);
  glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);

  GLint uvs_location = glGetAttribLocation(g_program_id, "uv");
  glEnableVertexAttribArray(uvs_location);
  CheckGLError("glEnableVertexAttribArray 2");
  glVertexAttribPointer(uvs_location, 2, GL_FLOAT, GL_FALSE, 0, 0);
  CheckGLError("glVertexAttribPointer 2");

  //GLuint texture_id;
  glGenTextures(1, &g_texture_id);
  CheckGLError("glGenTextures");
  glBindTexture(GL_TEXTURE_2D, g_texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  g_draw_buffer = (byte*)malloc(g_image_width * g_image_height * 4);
  if (!g_draw_buffer) {
    printf("Error allocating memory\n");
  }
  g_draw_buffer_ptr = g_draw_buffer;
  memset(g_draw_buffer_ptr, 0, g_image_width * g_image_height * 4);
  // for (unsigned int i = 0; i < g_image_width * g_image_height; i++) {
  //  *ptr = 0xFF;
  //  // *(ptr+1) = 0xFF;
  //  // *(ptr+2) = 0x00;
  //  ptr += 3;
  // }
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_image_width, g_image_height, 
    0, GL_RGBA, GL_UNSIGNED_BYTE, g_draw_buffer);

  Mat4 m, p, mvp;
  m.setIdentity();

  float right  =  1.0f;
  float left   = -1.0f;
  float top    =  1.0f;
  float bottom = -1.0f;

  // Projection matrix (ortographic)
  //  column-major order
  p.matrix[0] = 2.0f / (right - left);
  p.matrix[1] = 0.0f;
  p.matrix[2] = 0.0f;
  p.matrix[3] = 0.0f;

  p.matrix[4] = 0.0f;
  p.matrix[5] = 2.0f / (top - bottom);
  p.matrix[6] = 0.0f;
  p.matrix[7] = 0.0f;

  p.matrix[8]  = 0.0f;
  p.matrix[9]  = 0.0f;
  p.matrix[10] = 2.0f / (far - near);
  p.matrix[11] = 0.0f;

  p.matrix[12] = 0.0f;
  p.matrix[13] = 0.0f;
  p.matrix[14] = 0.0f;
  p.matrix[15] = 1.0f;

  glUseProgram(g_program_id);
  glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat*)p.matrix);
}


int _main_() {
  TCPSocket socket(Socket::Type::NonBlock);
  bool success = false;
  while (!success) {
    success = socket.connect("127.0.0.1", 14194);
    //success = socket.connect("81.202.4.30", 14194);
    //printf("Trying to connect...\n");
  }
  uint32_t buffer_size = 600000;
  byte* data = (byte*)malloc(buffer_size);
  memset(data, 2, buffer_size);
  socket.sendData(data, buffer_size);

  // while (true) {

  // }
  printf("Data sent\n");

  return 0;
}

void NetworkTask() {
  printf("Initializing network...\n");

  g_recv_data_buffer = (byte*)malloc(g_image_width * g_image_height * 4);
  g_recv_data_ptr = g_recv_data_buffer;

  bool success = false;
  bool ignore_sync_flag = false;
  while (!g_program_should_finish) {
    switch (g_network_state) {
      case NetworkState::NotConnected: {
        while (!success && !g_program_should_finish) {
          //success = g_socket.connect("81.202.4.30", 14194);
          success = g_socket.connect("127.0.0.1", 14194);
          // should reset success to false;
          //success = true;
        }

        g_network_state = NetworkState::Connected;

        break;
      }
      case NetworkState::Connected: {
        g_network_state = NetworkState::Receiving;
        printf("Connected!\n");

        break;
      }
      case NetworkState::Receiving: {
        uint32_t read_bytes = g_socket.receiveData(g_recv_data_ptr, g_image_width * g_image_height * 3);
        
        if (read_bytes == 0) {
          g_can_sync_network = false;
          ignore_sync_flag = true;
        }
        else {
          printf("Data received\n");
          g_network_state = NetworkState::Connected;
        }

        break;
      }
    }

    if (!ignore_sync_flag) {
      g_can_sync_network = true;
    }
    else {
      ignore_sync_flag = false;
    }
  }

  g_socket.close();
}

int main() {
  TCPSocket socket(Socket::Type::NonBlock);
  while (!socket.connect("127.0.0.1", 14194)) {

  }

  printf("Connected\n");

  byte i = 0;
  while (i < 255) {
    byte buffer[1024];
    memset(buffer, 0, 1024);
    while (socket.receiveData(buffer, 1024) == 0) {

    }
    printf("Data received[1]: %u\n", buffer[1]);

    ++i;
  }

  return 0;
}

int ___main() {
  signal(SIGINT, InterruptSignalHandler);

  std::thread network_thread(NetworkTask);

  InitializeGraphics();
  InitializeOpenGLStuff();

  byte r = 0;
  byte g = 64;
  byte b = 128;
  Chrono c;
  while (!glfwWindowShouldClose(g_window)) {
  	c.start();
    glClear(GL_COLOR_BUFFER_BIT);

   //  byte* ptr = g_draw_buffer;
	  // memset(ptr, 0, g_image_width * g_image_height * 4);
	  // for (unsigned int i = 0; i < g_image_width * g_image_height; i++) {
	  // 	*ptr      = r;
	  // 	*(ptr+1)  = g;
	  // 	*(ptr+2)  = b;
	  // 	*(ptr+3)  = 255;
	  // 	ptr += 4;
	  // }
	  // r++; g++; b++;
	  // r %= 255; g %= 255; b %= 255;
	  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
	  	g_image_width, g_image_height, GL_RGBA, GL_UNSIGNED_BYTE, g_draw_buffer_ptr);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    //CheckGLError("glDrawArrays");

    c.stop();
    //printf("Frame time: %.2f ms\n", c.timeAsMilliseconds());

    // Sync point
    // Swap between drawing buffer and the received buffer by network.
    // Only start to sync when the program is connected to the server, 
    //  because otherwise the program could be hung.
    if (g_can_sync_network == true && g_network_state != NetworkState::NotConnected) {
      printf("Waiting for network to end frame...\n");
      //network_thread.join();
      g_can_sync_network = false;
      if (g_draw_buffer_ptr == g_draw_buffer) {
        g_draw_buffer_ptr = g_recv_data_buffer;
      }
      else {
        g_draw_buffer_ptr = g_draw_buffer;
      }
    }

    glfwSwapBuffers(g_window);
    glfwPollEvents();
  }

  // CLEANUP
  network_thread.join();

  if (g_draw_buffer) {
    free(g_draw_buffer);
  }
  if (g_recv_data_buffer) {
    free(g_recv_data_buffer);
  }
  glfwDestroyWindow(g_window);
  glfwTerminate();

  return 0;
}