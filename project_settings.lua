#!lua

-- A solution contains projects, and defines the available configurations
solution "webcam-streaming"
  configurations { "Debug", "Release" }
  platforms { "native", "x64", "x32" }
  --startproject "Editor" -- Set this project as startup project
  location ( "./" ) -- Location of the solutions
         
  -- Project
  project "Client"
    kind "ConsoleApp"
    language "C++"
    location ( "./Client/" ) 
    targetdir ("./Client/bin/")

    --buildoptions("-stdlib=libstdc++")
    buildoptions_cpp("-std=c++11")
  
    buildoptions_objcpp("-std=c++11")
      
    includedirs { 
      "./Client/include", 
      "./Client/dependencies",
      "./Client/dependencies/glew/include/",
      "./Client/dependencies/GLFW/deps/",
      "./common/include/"
    }
      
    -- INCLUDE FILES
    files { -- GLEW
      group = "GLEW", "./Client/dependencies/glew/include/glew.c",
    }
    
    files{ group = "include", "./Client/include/**.h" } -- include filter and get the files
    files{ group = "src", "./Client/src/**.cc", "./Client/src/**.cpp", "./common/src/**.cpp" } -- src filter and get the files
    
    -- only when compiling as library
    --defines { "GLEW_STATIC" }
  
    -- configuration { "windows" }
    --   files {  -- GLFW
    --     group = "GLFW", "./Client/dependencies/GLFW/src/context.c", 
    --       "./Client/dependencies/GLFW/src/init.c", 
    --       "./Client/dependencies/GLFW/src/input.c",
    --       "./Client/dependencies/GLFW/src/monitor.c",
    --       "./Client/dependencies/GLFW/src/wgl_context.c",
    --       "./Client/dependencies/GLFW/src/win32_init.c",
    --       "./Client/dependencies/GLFW/src/win32_monitor.c",
    --       "./Client/dependencies/GLFW/src/win32_time.c",
    --       "./Client/dependencies/GLFW/src/win32_tls.c",
    --       "./Client/dependencies/GLFW/src/win32_window.c",
    --       "./Client/dependencies/GLFW/src/window.c",
    --       "./Client/dependencies/GLFW/src/winmm_joystick.c",
    --     --"./Client/dependencies/GLFW/include/GLFW/glfw3.h"
    --   }
    --   links {
    --     "opengl32"
    --   }
    --   defines { "__PLATFORM_WINDOWS__","_GLFW_WIN32", "_GLFW_WGL", "_GLFW_USE_OPENGL" }
    --   buildoptions_cpp("/Y-")
    --   windowstargetplatformversion "10.0.15063.0"
       
    configuration { "macosx" }
      files {  -- GLFW
        group = "GLFW", "./Client/dependencies/GLFW/src/context.c", 
          "./Client/dependencies/GLFW/src/init.c", 
          "./Client/dependencies/GLFW/src/input.c",
          "./Client/dependencies/GLFW/src/monitor.c",
          "./Client/dependencies/GLFW/src/nsgl_context.m",
          "./Client/dependencies/GLFW/src/cocoa_init.m",
          "./Client/dependencies/GLFW/src/cocoa_monitor.m",
          "./Client/dependencies/GLFW/src/mach_time.c",
          "./Client/dependencies/GLFW/src/posix_tls.c",
          "./Client/dependencies/GLFW/src/cocoa_window.m",
          "./Client/dependencies/GLFW/src/window.c",
          "./Client/dependencies/GLFW/src/iokit_joystick.m",
        --"./Client/dependencies/GLFW/include/GLFW/glfw3.h"
      }
      links  {
        "Cocoa.framework", "OpenGL.framework", "IOKit.framework", "CoreVideo.framework",
      }
      linkoptions { "-framework Cocoa","-framework QuartzCore", "-framework OpenGL", "-framework OpenAL" }
      defines { "__PLATFORM_MACOSX__", "_GLFW_COCOA", "_GLFW_NSGL", "_GLFW_USE_OPENGL" }
       
    configuration { "linux" }
      files {  -- GLFW
        group = "GLFW", "./Client/dependencies/GLFW/src/context.c", 
          "./Client/dependencies/GLFW/src/init.c", 
          "./Client/dependencies/GLFW/src/input.c",
          "./Client/dependencies/GLFW/src/monitor.c",
          "./Client/dependencies/GLFW/src/glx_context.c",
          "./Client/dependencies/GLFW/src/x11_init.c",
          "./Client/dependencies/GLFW/src/x11_monitor.c",
          "./Client/dependencies/GLFW/src/posix_time.c",
          "./Client/dependencies/GLFW/src/posix_tls.c",
          "./Client/dependencies/GLFW/src/x11_window.c",
          "./Client/dependencies/GLFW/src/window.c",
          "./Client/dependencies/GLFW/src/linux_joystick.c",
          "./Client/dependencies/GLFW/src/xkb_unicode.c",
        --"./Client/dependencies/GLFW/include/GLFW/glfw3.h"
      }
      links {
        "X11", "Xrandr", "Xcursor", "Xinerama", "Xi", "Xxf86vm", "rt", "pthread", "GL", "glut", "GLU", "m"
      }
      includedirs {
        "/usr/include/GL/"
      }
      libdirs {
        "/usr/bin/",
        "/usr/lib/"
      }
      defines { "__PLATFORM_LINUX__", "_GLFW_X11", "_GLFW_HAS_GLXGETPROCADDRESS", "_GLFW_GLX", "_GLFW_USE_OPENGL" }

    configuration "debug"
      defines { "DEBUG" }
      flags { "Symbols", "ExtraWarnings"}

    configuration "release"
      defines { "NDEBUG" }
      flags { "Optimize", "ExtraWarnings" }

  -- Project
  project "Server"
    kind "ConsoleApp"
    language "C++"
    location ( "./Server/" ) 
    targetdir ("./Server/bin/")

    --buildoptions("-stdlib=libstdc++")
    buildoptions_cpp("-std=c++11")
  
    buildoptions_objcpp("-std=c++11")
      
    includedirs { 
      "./Server/include/",
      "./common/include/"
    }
    
    files{ group = "include", "./Server/include/**.h" } -- include filter and get the files
    files{ group = "src", "./Server/src/**.cc", "./Server/src/**.cpp", "./common/src/**.cpp" } -- src filter and get the files
       
    configuration { "macosx" }
      defines { "__PLATFORM_MACOSX__" }
       
    configuration { "linux" }
      links {
        "pthread"
      }
      defines { "__PLATFORM_LINUX__" }

    configuration "debug"
      defines { "DEBUG" }
      flags { "Symbols", "ExtraWarnings"}

    configuration "release"
      defines { "NDEBUG" }
      flags { "Optimize", "ExtraWarnings" }