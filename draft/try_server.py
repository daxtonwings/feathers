import socket

def start_server(host='127.0.0.1', port=65432):
    # 创建 socket 对象
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        # 绑定 IP 和端口
        s.bind((host, port))
        # 监听连接
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

if __name__ == "__main__":
    start_server()