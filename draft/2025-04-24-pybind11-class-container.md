---
title: "笔记：pybind11（二） 类型传递方式"
---

## 一、class绑定时的持有模式

### 1.1 示例class

```c++
class Request
{
public:
    explicit Request(string request_id)
        : request_id_(std::move(request_id))
    {
        std::cout << "Construct: Request(" << request_id_ << ")\n";
    }

    ~Request()
    {
        std::cout << "Destruct: ~Request(" << request_id_ << ")\n";
    }

    Request(const Request& other)
        : request_id_(other.request_id_)
    {
        std::cout << "Copy Construct: Request(" << request_id_ << ")\n";
    }

    Request(Request&& other) noexcept
        : request_id_(std::move(other.request_id_))
    {
        std::cout << "Move Construct: Request(" << request_id_ << ")\n";
    }

    Request& operator=(const Request& other)
    {
        if ( this != &other ) {
            request_id_ = other.request_id_;
            std::cout << "Copy Assign: Request(" << request_id_ << ")\n";
        }
        return *this;
    }

    Request& operator=(Request&& other) noexcept
    {
        if ( this != &other ) {
            request_id_ = std::move(other.request_id_);
            std::cout << "Move Assign: Request(" << request_id_ << ")\n";
        }
        return *this;
    }

    const string& request_id() const noexcept
    {
        return request_id_;
    }

private:
    string request_id_;
};
```


### 1.2 pybind11按值绑定代码

```c++

Request get_request()
{
    return Request("value-id");
}

std::unique_ptr<Request> get_unique_request()
{
    std::cout << "get_unique_request()\n";
    return std::make_unique<Request>("unique-id");
}

std::shared_ptr<Request> get_shared_request()
{
    std::cout << "get_shared_request()\n";
    return std::make_shared<Request>("shared-id");
}
PYBIND11_MODULE(MODULE_NAME, m)
{
    py::class_<Request> request(m, "Request");
//    py::class_<Request, std::unique_ptr<Request>> request(m, "Request");
//    py::class_<Request, std::shared_ptr<Request>> request(m, "Request");
    request.def(py::init<string>())
        .def("request_id", &Request::request_id);

    m.def("get_request", &get_request);
    m.def("get_unique_request", &get_unique_request);
    m.def("get_shared_request", &get_shared_request);
}
```


pybind11对类型的不同持有绑定方式

1. `py::class_<Request>`
2. `py::class_<Request, std::unique_ptr<Request>>`
3. `py::class_<Request, std::shared_ptr<Request>>`


### 1.3 按值绑定与shared_ptr返回

```python
import example

def demo_request():
    req = example.get_shared_request()
    print(req.request_id())

demo_request()
print('exit')

>> get_shared_request()
>> Construct: Request(shared-request-id)
>> Destruct: ~Request(shared-request-id)
>> shared-request-id
>> Destruct: ~Request(shared-request-id)
>> Python(11689,0x207c3c840) malloc: *** error for object 0x600002104408: pointer being freed was not allocated
>> Python(11689,0x207c3c840) malloc: *** set a breakpoint in malloc_error_break to debug
```

上面代码中出现了2次析构，而且第二次析构导致了异常终止。

由于 get_shared_request()返回的是`std::shared<Request>`, pybind11会将器尝试转换为`Request`类型，
然后使用Python的对象管理模式去管理。

- pybind1自动将 shared_ptr<Request> 解引用成 Request&，然后再通过 Request 类型的构造器拷贝到 Python 的内部对象中
  - *但是，在试验代码中，未观察到Request的复制构造/移动构造*

如果只是按值绑定`py::class_<Request>`，那么在返回`shared_ptr<Request>` pybind11 会调用 PyObject_New + placement new 
在 Python 内部分配一块内存，然后 以“原始指针复制” 的方式， 直接调用构造函数构造对象。
如果 pybind11 找不到匹配的构造器（比如复制构造器、移动构造器），或者你没有绑定它，
**或者内部逻辑绕开构造函数（例如构造后立即 memcpy 或跳过初始化）**，你定义的拷贝/移动构造函数不会被调用
 

### 1.4 什么情况下按值绑定

什么情况下可以只用`py::class_<T>`，不用显示指定`py::class_<T, std::shared_ptr<T>>`？

**一、类的对象是轻量的、值语义的**

**二、返回时是按值对象的, 不是`T*`, `shared_ptr<T>`, `unique_ptr<T>`**

**三、不涉及继承/虚函数/多态**

使用 py::class_<T> 只支持单一对象，没有 holder 管理能力，**不能支持多态**

**四、对象的所有权不需要在c++与python间共享**


### 1.5 按shared_ptr绑定

```c++

Request get_request()
{
    return Request("request-id");
}

Request* get_request_ptr()
{
    return new Request("request-id");
}

std::unique_ptr<Request> get_unique_request()
{
    std::cout << "get_unique_request()\n";
    return std::make_unique<Request>("unique-request-id");
}

std::shared_ptr<Request> get_shared_request()
{
    std::cout << "get_shared_request()\n";
    return std::make_shared<Request>("shared-request-id");
}

PYBIND11_MODULE(MODULE_NAME, m)
{
    py::class_<Request, std::shared_ptr<Request>> request(m, "Request");
    request.def(py::init<string>())
        .def("request_id", &Request::request_id);

    m.def("get_request", &get_request);
    m.def("get_request_ptr", &get_request_ptr, py::return_value_policy::take_ownership);
    m.def("get_unique_request", [](/* no capture */) {
        return std::shared_ptr<Request>(get_unique_request().release());
    });
    m.def("get_shared_request", &get_shared_request);
}
```

1. 函数返回`Request`时，从Request移动构造python持有的shared_ptr
2. 函数返回`Request*`时，必须使用`py::return_value_policy::take_owership`
3. 函数返回`std::unique_ptr<Request>`时，需要修改绑定函数返回值，从unique_ptr转换为shared_ptr

### 1.6 按unique_ptr绑定

```c++
PYBIND11_MODULE(MODULE_NAME, m)
{
    py::class_<Request, std::unique_ptr<Request>> request(m, "Request");
    request.def(py::init<string>())
        .def("request_id", &Request::request_id);

    m.def("get_request", &get_request);
    m.def("get_request_ptr", &get_request_ptr, py::return_value_policy::take_ownership);
    m.def("get_unique_request", &get_unique_request);
    m.def("get_shared_request", &get_shared_request);
}
```


1. 返回值为`Request`时，pybind11内部转换`return std::unique_ptr<Request>(new Request(get_request()));`
2. 返回值为`shared_ptr<Request>`时，将发生内存问题。


## 二、类型作为函数传入类型的讨论

### 2.1 基本示例

```c++
class Request
{
public:
    explicit Request(string request_id)
        : request_id_(std::move(request_id))
    {
        std::cout << "Construct: Request(" << request_id_ << ")\n";
    }
    
    // 构造/析构/赋值 函数均std::out输出
    
    const string& request_id() const noexcept
    {
        return request_id_;
    }

private:
    string request_id_;
};


void do_request_ref(const Request &req){
    std::cout<<"do() const Request& "<<req.request_id()<<"\n";
}
void do_request(Request req){
    std::cout<<"do() Request "<<req.request_id()<<"\n";
}
void do_request_ptr(Request* req){
    std::cout<<"do() Request* "<<req->request_id()<<"\n";
}
void do_request_shared(std::shared_ptr<Request> req){
    std::cout<<"do() shared<Request> "<<req->request_id()<<"\n";
}
void do_request_shared_ref(const std::shared_ptr<Request>& req) {
    std::cout<<"do() const shared<Request>& "<<req->request_id()<<"\n";
}
// pybind无法绑定该函数 
//  m.def("do_request_unique", &do_request_unique)
void do_request_unique(std::unique_ptr<Request> req){
    std::cout<<"do() unique_ptr<Request> "<<req->request_id()<<"\n";
}
// pybind无法绑定该函数 
// m.def("do_request_unique_ref", &do_request_unique_ref)
void do_request_unique_ref(const std::unique_ptr<Request>& req) {
    std::cout<<"do() const unique_ptr<Request>& "<<req->request_id()<<"\n";
}
```

### 2.2 使用按值绑定Request

按值类型绑定Request，`py::class_<Request> request(m, "Request");`

#### (Request req)

`do_request(Request req)`，参数传入时发生「复制构造」返回时「析构」
 
#### (const Request& req)

`do_request_req(const Request& req)` 正常执行

#### (Request* req)

 `do_request_ptr(Request* req)`，Python将持有的PyObject地址传给c++函数

`py::class_<Request> request(m, "Request");`
没有指定智能指针，仅是值类型。然后你在 Python 侧调用了
```python
req = Request("py-req")
do_request_ptr(req) 
```
结果，C++ 接收到的是 Request* 类型，且运行正常


尽管 py::class_<Request> 绑定的是值类型，Pybind11 在内部会自动分配并管理 Python 拥有的 Request 实例。
也就是说： 当 Python 创建了一个 Request("py-req") 对象后，这个对象背后是一个 在堆上动态分配的 Request 实例。
Python 持有这个对象的控制权。
当你把这个对象传入 Request* 参数时，Pybind11 会做一次安全的转换：
**将 Python 中封装的 Request 实例的裸指针传入 Request* 参数中。**
这是 Pybind11 对 T* 和 T& 参数的默认行为，它会尝试将 Python 持有的对象（即 py::object）内部的 C++ 实例地址拿出来，传给你。

不过，`void do_request_ptr(Request* req);`这个函数拿到的是裸指针，它不拥有该指针所指向的对象的生命周期

#### (std::shared_ptr<Request> req)

`do_request_shared(std::shared_ptr<Request> req)`, python运行时异常

**RuntimeError: Unable to load a custom holder type from a default-holder instance**

你在 Python 里传入了一个由默认持有者（值类型）创建的对象，
但在 C++ 函数签名中，你要求传入的是智能指针持有的对象（如 std::shared_ptr<Request>），
pybind11 没办法把这两种“持有者”安全转换。

#### (std::unique_ptr<Request> req)

`do_request_unique(std::unique_ptr<Request> req)` 绑定失败

你遇到的是 Pybind11 对 std::unique_ptr<T> 参数传入方式的限制 —— 
它确实不能直接从 Python 传递一个 T 实例作为 std::unique_ptr<T> 参数，除非你在绑定时做了特殊处理。这是因为
- Python 中的对象生命周期是由 Python 管理的，你无法从一个 Python 持有的对象中「生成唯一所有权」给 C++（unique_ptr）。
- std::unique_ptr<T> 表示独占所有权转移 —— 但 Python 中并不拥有一个 unique_ptr<T> 实例来「转移」。
