---
title: "Python asyncio 协程并发"
---

前文背景：「[异步：从并发到Python协程](https://daxtonwings.github.io/feathers/2024/04/19/from-concurrency-to-python-coroutine.html)」

* **[以TCP服务为例分析](#以TCP服务为例分析)**
  * [TCP](#tcp服务的简单实现)
  * [多线程并发的TCP服务实现](#多线程并发的tcp服务实现)
  * [附加合并消息处理的tcp服务实现](#附加合并消息处理的tcp服务实现)
  * [asyncio协程的tcp服务实现](#asyncio协程的tcp服务实现)

## 以TCP服务为例分析

### TCP服务的简单实现

实现一个TCP echo服务，需进行这些动作：
（1）建立与配置socket，绑定到指定端口号`bind()`，并在端口上监听连接信息`listen()`；
（2）等待对端（客户端）的连接建立`accept()`；
（3）在建立的连接对象上，接收与发送数据`recv()`, `sendall()`

下面这是python实现的简单的TCP echo服务，特点是“单线程顺序执行”服务模式。意味着：

从正面角度，不同的系统函数需要按照特定的顺序执行，以保证应用逻辑的正确性。
例如系统调用函数`accept()`, `recv()`, `sendall()`是按代码顺序依次执行的。
`recv()`在`accept()`之后是因为连接之后才能接收数据，
`sendall()`在`recv()`之后是因为接收数据之后才能echo响应，此时代码的逻辑顺序是必要的。

从反面角度，顺序执行的代码前后有强约束性，数据方面，前置代码的结果输出是后序代码的输入；
隐性方面，前置代码的运行时间约束了后序代码的开始执行时间。例如：
在`accept()`返回连接对象`conn`后，至连接对象关闭之前，将循环执行`recv()`与`sendall()`操作。
服务无法对其它客户端的连接执行`accept()`。

```python
import socket


def start_server(host='127.0.0.1', port=65432):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, port))
        s.listen()
        print(f"Server started at {host}:{port}")
        # 接受连接
        while True:
            conn, addr = s.accept()
            with conn:
                print(f"Connected by {addr}")
                while True:
                    # 接收数据
                    data = conn.recv(1024)
                    if not data:
                        break
                    print(f"Received: {data.decode()}")
                    # 发送数据
                    conn.sendall(data)


def start_client(host='127.0.0.1', port=65432):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        s.sendall(b'Hello, world')
        data = s.recv(1024)
    print(f"Received {data!r}")


if __name__ == "__main__":
    start_server()
```

### 多线程并发的TCP服务实现

代码逻辑的“顺序性”有时是实现功能正确性的必要条件，有时是拖累代码执行的无效约束。
消除“单线程顺序执行”TCP echo服务无效约束的方式，简单而言，就是打破约束条件。

下面代码实现的是，多线程顺序执行TCP echo服务。
主要的改进在于`accept()`建立了与客户端的连接对象后，交由另一个线程执行`recv()`与`sendall()`动作。
如此，即保证了`accept()`，`recv()`，`sendall()`的逻辑顺序。由可以立即处理下一次连接请求`accept()`。

好的消息是，多线程提供的并发解决方案，可以让我们打破无效“顺序性”代码约束的问题。
坏的消息是，多线程创建与使用需要额外的成本开销，而且多线程在系统上的执行顺序对于上层应用是不可控制的。
当应用变得复杂后，多线程的代码必需使用锁、信号量、消息等同步工具实现完备的功能。
这些同步工具进一步增加了多线程代码运行时的成本开销。

```python
import socket
import threading


def handle_client(conn, addr):
    print(f"Connected by {addr}")
    with conn:
        while True:
            data = conn.recv(1024)
            if not data:
                break
            print(f"Received from {addr}: {data.decode()}")
            conn.sendall(data)


def start_server(host='127.0.0.1', port=65432):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, port))
        s.listen()
        print(f"Server started at {host}:{port}")

        while True:
            conn, addr = s.accept()
            client_thread = threading.Thread(target=handle_client, args=(conn, addr))
            client_thread.start()


if __name__ == "__main__":
    start_server()
```

### 附加合并消息处理的TCP服务实现

对多线程并发的TCP服务实现上增加这样一个需求：
从所有客户端连接`conn`接收到的数据合并记录相同的一个队列，并由对这个队列进行加工处理。
合并消息处理的这个需求，比简单的TCP echo服务只稍微复杂了一点点。
但这一点点对多线程并发实现而言，需要使用同步原语（`threading.Lock()`）锁，保证并发逻辑的正确性：

- 记录：各客户端记录数据时，要获取锁
- 读取：处理线程获取数据时，要获取锁

```python
import socket
import threading

message_list = []
lock = threading.Lock()


def handle_client(conn, addr):
    print(f"Connected by {addr}")
    with conn:
        while True:
            data = conn.recv(1024)
            if not data:
                break
            with lock:
                message_list.append((addr, data))
            conn.sendall(data)


def process_messages():
    while True:
        with lock:
            if message_list:
                addr, data = message_list.pop(0)
                print(f"Processed message from {addr}: {data.decode()}")


def start_server(host='127.0.0.1', port=65432):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, port))
        s.listen()
        print(f"Server started at {host}:{port}")

        # 启动消息处理线程
        processing_thread = threading.Thread(target=process_messages, daemon=True)
        processing_thread.start()

        while True:
            conn, addr = s.accept()
            client_thread = threading.Thread(target=handle_client, args=(conn, addr))
            client_thread.start()


if __name__ == "__main__":
    start_server()
```

### asyncio协程的TCP服务实现

首先，回顾一下多线程方案的TCP服务实现：
（1）多线程并发方案，打破了单线程代码存在的无效顺序关系约束；
（2）多线程操作相同数据时，需要同步原语（锁、信号量、消息）的保护。

之前被忽略的是，`threading.Thread()`这里的“线程”概念，是随着计算机系统演化才诞生的。
在操作系统提供“线程”API的支持后，应用程序才有可能按照多线程并发的方案开发。
不过，线程的调度是由操作系统决定的，某个线程什么时候开始运行，运行多长时间等，应用程序无法自主控制。

理论上说，操作系统也是一种代码程序软件。
既然都是程序软件，操作系统能够在系统内核实现的执行代码逻辑的并发，
上层应用程序当然也可以自主执行代码逻辑的并发。
`asyncio`协程，正式完成了这样的并发功能。
当然，`asyncio`也依赖操作系统特定API的功能支持，最核心的是“I/O多路复用”API

**I/O多路复用**：用于同时监视多个文件描述符（如套接字、文件、管道等）
并在任何一个文件描述符就绪（可读、可写、异常）时通知应用程序。
这种机制允许单个线程有效地处理多个IO操作，而不必为每个IO操作创建一个线程或进程，
从而提高了应用程序的并发性能和资源利用率。

简单来说，原来的I/O操作（`recv()`等）操作系统和应用层的动作是一体的；
在I/O多路复用的视角下，操作系统准备I/O数据的操作和应用层的I/O动作分离了。

```python
import asyncio
import socket


async def handle_client(loop, client_socket, message_list):
    addr = client_socket.getpeername()
    print(f"Connected by {addr}")
    while True:
        data = await loop.sock_recv(client_socket, 1024)
        if not data:
            break
        await message_list.put(data)
        await loop.sock_sendall(client_socket, data)
    client_socket.close()


async def process_messages(message_list: asyncio.Queue):
    while True:
        addr, data = await message_list.get()
        print(f"Processed message from {addr}: {data.decode()}")


async def start_server(host='127.0.0.1', port=65432):
    # asyncio 事件循环：应用层并发调度管理
    loop = asyncio.get_running_loop()
    message_list = asyncio.Queue()
    loop.create_task(process_messages(message_list))

    # 创建socket，设置非阻塞模式
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((host, port))
    server_socket.listen()
    server_socket.setblocking(False)
    print(f"Server started at {host}:{port}")

    while True:
        client_socket, addr = await loop.sock_accept(server_socket)
        # 创建单独处理客户端连接的协程并发        
        loop.create_task(handle_client(loop, client_socket, message_list))


if __name__ == '__main__':
    asyncio.run(start_server())
```

这段代码展示了在单线程，`asyncio`使用多协程实现并发逻辑的代码。
而且，为了细节代码展示，使用了较底层的操作方法`loop = asyncio.get_running_loop()`获取事件循环。
与多线程的TCP服务实现相比，`asyncio`协程实现有以下特点：

1. 线程并发需要创建线程`threading.Thread()`，协程并发需要创建协程任务`loop.create_task()`；
2. 所有协程在单线程中执行，共享数据的读写操作无需锁保护；
3. 所有I/O操作均运行在非阻塞模式：
   - I/O多路复用保证在有待处理的I/O数据的时候才执行I/O动作。
   - 若I/O操作时无有效数据，非阻塞模式I/O会立即返回错误信息，不会阻塞其它代码的执行。


