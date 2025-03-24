
cmake显示编译echo_server并连接编译好的asio

```bash
cd /Users/abakus/c-projects/asio/build/asio/src/examples/cpp20 && \
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++  \
-I/Users/abakus/c-projects/asio/asio/src/examples/cpp20/../../../include \
-std=gnu++20 -arch arm64 -isysroot \
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.5.sdk \
-MD -MT asio/src/examples/cpp20/CMakeFiles/echo_server.dir/coroutines/echo_server.cpp.o \
-MF CMakeFiles/echo_server.dir/coroutines/echo_server.cpp.o.d \
-o CMakeFiles/echo_server.dir/coroutines/echo_server.cpp.o \
-c /Users/abakus/c-projects/asio/asio/src/examples/cpp20/coroutines/echo_server.cpp
```

```bash
cd /Users/abakus/c-projects/asio/build/asio/src/examples/cpp20 && \
/Applications/CMake.app/Contents/bin/cmake -E cmake_link_script \
CMakeFiles/echo_server.dir/link.txt --verbose=1

/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++  \
-arch arm64 -isysroot \
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.5.sdk \
-Wl,-search_paths_first \
-Wl,-headerpad_max_install_names CMakeFiles/echo_server.dir/coroutines/echo_server.cpp.o -o echo_server   \
-L/Users/abakus/c-projects/asio  -Wl,-rpath,/Users/abakus/c-projects/asio -lasio
```

libasio.dylib链接时`-Wl,-rpath,/Users/abakus/c-projects/asio -lasio`


使用`otool -L`查看执行程序依赖的动态库
(linux `ldd`)

```bash
> otool -L asio/src/examples/cpp20/echo_server
asio/src/examples/cpp20/echo_server:
        @rpath/libasio.1.30.2.dylib (compatibility version 0.0.0, current version 1.30.2)
        /usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1700.255.5)
        /usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1345.120.2)
```

使用`otool -l`查看rpath信息 (linux `readelf -d`)
```bash
> otool -l asio/src/examples/cpp20/echo_server | grep RPATH -A2
          cmd LC_RPATH
      cmdsize 48
         path /Users/abakus/c-projects/asio (offset 12)
```

`gcc -g -shared -Wl,-Bsymbolic -o libfoo.so foo.o`使用`-Bsymbolic`链接器选项表示共享库中全局符号优先在库中搜索定义

`-Wl,-Bstatic`显示指定链接静态库 `-Wl,-Bdynamic`显示指定链接动态库