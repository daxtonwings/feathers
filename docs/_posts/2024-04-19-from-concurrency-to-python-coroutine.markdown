---
title: "异步：从并发到Python协程"
---

* **[从计算机原理开始](#从计算机原理开始)**
    * [什么是并发](#什么是并发)
    * [为什么并发](#为什么要并发)
    * [如何并发](#如何并发)
        * [进程、线程、协程](#进程线程协程)
        * [函数调用栈](#函数调用栈)
* **[协程](#协程)**
* **[Python协程](#python协程的进化)**
  * [PEP 255：简单生成器](#pep-255简单生成器)
  * [PEP 342：增强版生成器](#pep-342增强版生成器)
    * [动机与改进思路](#动机与改进思路)
    * [语法变化](#语法变化)
    * [完善资源管理与异常控制](#完善资源管理与异常控制)
    * [yield语句：逆向函数](#yield语句逆向函数)
  * [PEP 380：PEP代理生成器](#pep-380代理生成器)
    * [动机与改进思路](#动机与改进思路-1)
    * [yield from语句概要](#yield-from语句概要)
    * [yield from实现细节](#yield-from实现细节)
    * [语法糖与优化](#语法糖与优化)
  * [PEP 492：Python协程](#pep-492python协程)
    * [历史背景与Python协程的诞生](#历史背景与python协程的诞生)
    * [站在开发使用角度的动机](#站在开发使用角度的动机)
    * [协程定义](#协程定义)
    * [await 表达式](#await表达式)
    * [async with/async for](#async-withasync-for)
* **[后记](#后记)**


## 从计算机原理开始

> 参考 「深入理解计算机系统Computer System」

### 什么是并发

术语**并发(concurrency)**，指系统能同时处理多个任务的能力。
多个任务动作并非同时发生，是系统通过在多个任务间频繁切换运行，模拟达到多个任务同时运行的效果。
并发强调的是系统对多任务的交替执行与管理。
例如，一个单核CPU上运行了多个进程任务，每个任务运行一段时间后会被暂停以开始其他任务执行，
并在未来某时刻继续运行。

术语**并行(parallelism)**，指系统在同一时刻同时执行多个任务，强调的是多个任务在同一时刻同时执行。
例如，一个双核CPU上同时分别运行了进程A和进程B。

并发与并行的概念虽有区别，但非相互对立。并发是一种达到并行的手段，并行也要靠并发管理。
场景一，操作系统执行I/O操作时会切换至其他任务运行，
对于CPU而言是暂停了I/O操作开始了其他的操作，但对于系统而言，I/O操作和其他操作是并行的。
场景二，现代CPU在执行多控制流任务时，在等待数据装载至高速缓存时可以继续执行其它控制流任务。
场景三，现代CPU采用了流水线执行形式，实际上指令不是独占式的排队执行的，而是同时处理多条指令。


### 为什么要并发

反向思考：如果系统只能依次处理单个任务，不能并发会怎么样？

- 情景一：鼠标点击启动应用程序时，直到应用程序退出前鼠标将毫无响应；
- 情景二：在保持文件时，整个系统等待硬盘的操作；

当下，计算机系统中到处存在着并发，使用计算机的我们习惯的认为并发是系统自然的结果。
但事实上，所有的并发都得由程序特别实现的处理逻辑，
比如在UI操作代码中要启动多个线程分别处理前、后端的执行逻辑，这样底层的运行不会造成前端UI无响应。

不过，并发的核心目标是要让系统的每个组件（CPU、GPU、硬盘）避免空闲、高效运转，
是要榨取高性能组件的闲置能力。而非通过并发把一个低性能的CPU变成一个高性能的CPU。


### 如何并发

#### 进程、线程、协程

计算机的并发场景很多，应用开发中可以控制的并发主要有进程、线程、协程三种形式。

**进程**：进程是操作系统对正在运行的程序的抽象。
一个操作系统上可以同时运行多个进程，每个进程都好像独占的使用硬件资源。
无论是单核还是多核系统中，一个CPU看上去都像是在并发的执行多个进程，这是通过处理器在进程间切换实现的。
操作系统实现这种交错执行的机制称为“**上下文切换**”。
上下文包括许多信息，比如PC(程序计数器)和CPU寄存器，以及主存储器的内容。
当操作系统决定要把控制权从当前进程转移到另一个进程时，就会进行“上下文切换”。

进程的上下午切换通常发生在应用程序执行系统调用(system call)指令时。
当执行系统调用指令时，比如读写文件，应用层代码将控制权传递给内核。
内核根据一些条件（进程的累计运行时间，系统调用是否阻塞）选择判断，
是继续执行当前进程，还是切换执行其它进程。

**线程**：线程是比进程轻量的调度单元，不同操作系统有各自的进程-线程模型。
有些操作系统在内核中负责线程调度，有些操作系统将线程调度留给应用层实现。
对于Linux来说，线程和进程一样是在操作系统内核中调度的。
这里，线程比进程更轻量是指，在相同进程的多个线程之间切换时，需要切换的上下文信息更少。
比如：线程不需要切换整个虚拟内存空间，仅需要切换线程运行栈信息。

**协程**：协程是更加轻量级的调度单元，通常是在应用层调度，而不是在内核中调度。
不过，在不同的开发语言中，协程表示的具体含义有较大的区别。
比如：c++20标准中的coroutine特指无栈协程，其调度控制由编译器根据协程代码补充实现；
go中的协程指goroutine，是MPG模型的基本调度单元，由go的运行时环境负责调度；
python中的协程指由async语法定义的函数，由内置asyncio模块依赖I/O多路复用的方式调度。


#### 函数调用栈

无论高级语言多么复杂，对底层CPU来说全部要还原至指令语句。
指令语句组合形成的首个高级抽象单元是“过程”，在高级语言中是function、subroutine等。
当发生过程调用时，比如过程P调用了过程Q，有一些必要的动作，包括：
（1）保持当前过程P的信息，（2）准备过程Q的调用栈，（3）更新“程序计数器”PC转移下一个指令语句位置。
当发生过程调用命令`call Q`时，
首先将返回指令地址(即过程P的下一条指令地址)压入栈中, 并更新栈指针为新的过程Q在内存栈上分配空间。
然后, 将PC设置为过程Q的起始指令位置开始执行。
最后，当过程Q执行完毕时，通过回退栈指针清除过程Q的栈帧，
接着从栈中弹出过程P的返回指令地址作为PC值，从而继续执行过程P。

协程，也属于过程，需要有自己的内存空间。
若在内存栈上分配协程存储空间时，称为有栈协程；
若在内存堆上分配协程存储空间时，称为无栈协程。


## 协程

重新正式地为协程给出定义：

协程是一个计算机过程，
这个过程允许被**暂停**及**恢复**，从而实现了协作式的多任务并发。

- 与传统过程相比，协程允许暂停与恢复
- 与多进程、多线程的并发时控制权被动切换相比，多协程的控制权切换是主动转移的，发生在协程的暂停位置，
因此多协程实现的是协作式的并发。与之相对，多进程、多线程是抢占式并发。


## Python协程的进化

### PEP 255：简单生成器

> [PEP 255-SimpleGenerators](https://peps.python.org/pep-0255/)
> 关键字yield (2001-05-18)

第一代生成器(generator)功能简单，下面以Gen1表示第一代生成器。
含有关键字`yield`语句的python函数是生成器。
与普通函数的区别在于，Gen1可以在`yield`语句处暂停与恢复执行。


```python
def g():
    i = 1
    yield i
    i += 1

a = g()  # a is an Iterator
a.__next__()  # next(a)
a.__next__()  # next(a)
```

- `a=g()`为生成器内变量准备储存空间, a是一个迭代器
- `yield`表示函数主动让出执行条件，保存当前状态
- `a.__next__()`: 
  - 第一次执行时进入协程运行至`yield i`返回。
  - 第二次执行时从`yield i`下一条语句继续执行，运行至函数结束
- 当生成器运行至结束/`return`语句时, 产生`StopIteration`异常表示到达迭代器结尾


### PEP 342：增强版生成器

> [PEP 342 – Coroutines via Enhanced Generators](https://peps.python.org/pep-0342/)
> 改进yield语句功能 (2005-05-10)

#### 动机与改进思路

第一代生成器Gen1的函数过程允许的暂停与恢复，几乎符合协程的定义。
不过通用python函数相比，Gen1有以下不足：

1. Gen1恢复执行时，不能更新状态。
协程暂停时通常是为了等待某个异步事件的更新，以函数调用类比，函数P调用函数Q后可以从Q中获取返回值信息。
Gen1初始化后无法从外部获取信息，只能按照唯一特定的逻辑执行。
 
2. Gen1恢复执行时，不能传入异常。
与第一点类似，Python的函数调用可能会产生异常，调用者要妥善处理异常或者继续传递异常。
Gen1无法更新外部信息，也无法获知异常情况

3. Gen1的`yield`语句不能位于try/finally块的try部分。

在PEP 342中，改进的第二版generator（下面以Gen2表示）主要特性变化包括：
更改了`yield`语句, 新增类型方法`send()`/`throw()`/`close()`, 完善资源管理以符合异常处理需要。

#### 语法变化

从语法上`yield`语句由声明语句改为表达式语句。`yield`改为表达式语句意味着Gen2能够在恢复运行时更新数据。

新增类型方法`send(value=None)`，
`gen2.send()`用于开始/恢复生成器执行，可替代原来的迭代器语句`gen2.__next__()`并具有更新数据功能。
`gen2.send()`函数的返回值与`gen2.__next__()`相同，为`yield`关键字后面的表达式的值。
返回异常时，当运行至结尾时引发`StopIteration`，或者generator运行中引发的其它异常。
若调用时generator已经结束，`send()`方法会收到异常

新增类型方法`throw(type,value=None,tb=None)`
`gen2.throw()`用于向生成器传递类型为`type`的异常信息，
等效于在`yield`语句引起异常`raise type, value, tb`。
`throw()`方法的返回值与`send()`类似，可能返回后续`yield`后面的表达式值，也可能收到异常。
若调用时generator已结束，`throw()`直接收到其传入的异常类型。

新增类型方法`close()`及新异常类型`GeneratorExit`。
`gen2.close()`等效于向Gen2传递`GeneratorExit`异常。
由调用者向生成器传入时控制生成器中止，
由生成器向外传播时与`StopIteration`相似表示生成器结束。


#### 完善资源管理与异常控制

Gen2的`yield`语句可以位于try/finally语法块的try部分。
为此，当Gen2被触发垃圾回收时，`gen2.__del__()`将检查并确保`gen2.close()`被调用。
`close()`实现类型如下逻辑代码

```python
def close(self):
    try:
        self.throw(GeneratorExit)
    except (GeneratorExit, StopIteration):
        pass
    else:
        raise RuntimeError("generator ignored GeneratorExit")
```

如果`close()`动作在Gen2中没有妥善处理， 既未抛出`GeneratorExit`/`StopIteration`时，
垃圾回收逻辑向std.err输出异常错误信息后忽略该异常情况。

#### yield语句：逆向函数

Gen2的`yield`语句可以看作是一种逆向函数调用，
函数的输入是`yield`右侧表达式的数值, 函数的返回值是`yield`语句的返回值。
这个逆向函数的具体功能实现是由调用者两次`send()`之间的逻辑实现的。


### PEP 380：代理生成器

> [PEP 380 - Syntax for Delegating to a Subgenerator](https://peps.python.org/pep-0380/)
> yield from语句(2009-02-13)

#### 动机与改进思路

增强版生成器Gen2，从功能逻辑角度是正确完备的。
从开发使用角度，Gen2的表达能力不足，在复杂的场景下，仅使用Gen2实现处理逻辑代码十分复杂。

协程的关键能力是暂停与恢复，当程序执行到阻塞操作时主动暂停执行让出操作控制权，
随后等待阻塞操作完成时被唤醒恢复。
考虑一个协程函数，要依次进行文件读取、Http请求两种阻塞操作。
其中文件读写、Http请求已有封装好的协程。

使用generator实现该协程函数，意味着主函数gen_m要依次运行文件读取协程gen_file与Http请求协程gen_http。
当gen_file进行文件读取时，整个协程gen_m+gen_file暂停，读取完毕后恢复运行。
当gen_http发出请求等待响应时，z整个协程gen_m+gen_http暂停，随后在恢复运行。 
若使用Gen2实现，代码片段如下。这里的代码片段只实现了主体逻辑，分支情况未完整实现。

```python
def gen_file():
    # wait for file reading
    file_value = yield None
    # do something
    res = foo(file_value)
    raise StopIteration(res)

def gen_http():
    http_request = ...
    # wait for response
    resp = yield http_request
    return resp

def gen_m():
    _file_coroutine = gen_file()
    file_pause_flag = _file_coroutine.send()
    file_value = yield file_pause_flag
    try:
        _file_coroutine.send(file_value)
    except StopIteration as e:
        res = e.args[0]
    else:
        raise RuntimeError
    
    _http_coroutine = gen_http()
    http_request = _http_coroutine.send()
    resp = yield http_request
    try:
        _http_coroutine.send(resp)
    except StopIteration as e:
        res = e.args[0]
    else:
        raise RuntimeError
```

上述过程中当gen_file暂停意味着gen_m一起暂停，表现为gen_m收到gen_file的yield数据时，立即向上yield。
随后文件读取完成时，gen_m收到了外层的恢复数据后再向内部的gen_file转发。 
可以看出gen_m在gen_file暂停与恢复时，只是一个转发的角色，却每次都要实现一遍类型的代码。
为了改进generator在这种层级调用场景下的表达能力。第三代generator增加了`yield from`表达式语法。


#### yield from语句概要

包含`yield from`语句的函数是生成器。
语句`yield from <expr>`要求`expr`是迭代器。
在生成器运行期间，`expr`迭代器直接与生成器的调用者交互，
含有`yield from`的生成器发挥代理作用。
进一步，generator初始化后就是迭代器，可以作为`yield from`语句的迭代对象。
下面将`yield from`所在的生成器称为GenD（delegating generator，代理生成器），
将`<expr>`部分的生成器称为GenS（subgenerator，子生成器）。

那么， 当有值从GenS生成(yield)时，GenD直接将该值yield给GenD的调用者，
`yield from`表达式不发生求值处理。
随后，当GenD的调用者调用`send()`/`throw()`方法时，GenD将相关参数转发给GenS。
最后，当GenS运行至返回时，`return`语句返回值是`yield from`表达式的结果。

在Gen2中，我们将`yield`语句看作是以生成值为输入，以`send()`为返回的逆向函数。
上层调用者是该逆向函数的函数结构体。
在Gen3中，具有`yield from`语句的生成器是这个函数结构体的中间部分，
实现了函数层级封装与调用的效果。

#### yield from实现细节

从`expr`迭代器yield的数值直接传递给GenD的调用者

调用`genD.send(value)`时, 
若value为None，则调用迭代器的`__next__()`方法，反之调用迭代器的`send()`方法。
若迭代器抛出`StopIteration`时，GenD恢复运行，对`yield from`语句求值，
若迭代器抛出其它异常时，在GenD的`yield from`语句中引起该异常。

调用`genD.throw()`当传递的不是`GeneratorExit`异常，则直接调用迭代器的`throw()`方法。
若迭代器抛出`StopIteration`时，GenD恢复运行，对`yield from`语句求值，
若迭代器抛出其它异常时，在GenD的`yield from`语句中引起该异常。

调用`genD.throw()`传递`GeneratorExit`，或者调用`genD.close()`时，
将尝试调用迭代器的`close()`方法。
随后，若迭代器抛出异常，则在`yield from`语句处引发该异常；
若迭代器未抛出异常，则在`yield from`语句处引发`GeneratorExit`异常。

`yield from`语句求值的结果，是迭代器结束运行时抛出的`StopIteration`的首个参数。

生成器返回时，返回语句`return <expr>`将引发`StopIteration(expr)`格式异常。 

#### 语法糖与优化

`yield from`的语句`RESULT = yield from EXPR`，等效于下面代码实现。
从这个角度看，`yield from`就是层级调用generator的某种语法糖。
但是，python对`yield from`实现也不只是简单的语法糖替换，
通过对内部generator的缓存管理，能够压缩`send()`与`yield`语句的传递路径，优化代码效率。

```python
# RESULT = yield from EXPR
_i = iter(EXPR)
try:
    _y = next(_i)
except StopIteration as _e:
    _r = _e.value
else:
    while 1:
        try:
            _s = yield _y
        except GeneratorExit as _e:
            try:
                _m = _i.close
            except AttributeError:
                pass
            else:
                _m()
            raise _e
        except BaseException as _e:
            _x = sys.exc_info()
            try:
                _m = _i.throw
            except AttributeError:
                raise _e
            else:
                try:
                    _y = _m(*_x)
                except StopIteration as _e:
                    _r = _e.value
                    break
        else:
            try:
                if _s is None:
                    _y = next(_i)
                else:
                    _y = _i.send(_s)
            except StopIteration as _e:
                _r = _e.value
                break
RESULT = _r

# StopIteration
class StopIteration(Exception):
    def __init__(self, *args):
        if len(args) > 0:
            self.value = args[0]
        else:
            self.value = None
        Exception.__init__(self, *args)
```

### PEP 492：Python协程

> [PEP 492 – Coroutines with async and await syntax](https://peps.python.org/pep-0492/)
> python协程与async/await语句(2015-09-15)


#### 历史背景与Python协程的诞生

随着计算机技术的发展，互联网爆发式的应用增长，引发了响应式、易扩展的代码需求。
协程正好在这样的场景下能够发挥巨大的威力。

简单生成器（PEP 255）、增强版生成器（PEP 342）、代理生成器（PEP 380）
为Python提供了可暂停及恢复的函数过程实现方式——生成器generator。
若把视角放大些，正如本文一开始介绍并发时所说，协程实现系统并发的轻量级调度单元。
而以协程实现系统并发任务的目标，仅有协程本身是不够的，还需要有相应的完善的调度系统支持。

为了实现高效协程并发的开发模式， Python在借鉴了已基本成熟的generator的经验基础上，
在“协程”的实现上向前迈出了关键的一步：

1. 新增了`async/await`等关键字标识用以定义协程，并和传统generator模式区分
2. 标准化的定义了Python协程调度的实现接口——事件循环（EventLoop）

上述选择并非Python创新性的定义与实现，是Python综合考虑了许多协程实践经验所做的选择。
这种选择无疑的奠基性、决定性的。由于代码开发模式一旦确定，后面将很难做出不兼容的调整改变。
也如PEP 492作者所说：
“我们相信，这里提出的更改将有助于保持 Python 在快速增长的异步编程领域的相关性和竞争力，
正如许多其他语言已经采用或计划采用的那样。”

#### 站在开发使用角度的动机

从python语言的开发使用角度，使用generator作为Python协程的实现方案有以下不足：

- 生成器(generator)与协程(coroutine)的代码定义易混淆
- 是生成器(generator)还是普通函数(function)的区别是有函数体内语句`yield`/`yield from`所决定，很有可能造成微妙的不易察觉的错误代码。
- `yield`/`yield from`表达式，在python仍有一些约束，比如不能用于`with`后`for`等语句。这也限制了协程的实现开发。


#### 协程定义

**协程(coroutine)**，由`async def`定义的函数就是协程（下文用Coro表示）。
Coro的实现是基于generator语法的，但与传统generator对比, Coro有明显的不同:
- Coro函数体中不能有`yield`/`yield from`语句
- 传统generator初始化产生的对象类型是生成器Generator，Coro初始化产生的对象类型是新类型Coroutine
- 传统generator具有`__iter__()`与`__next__()`特殊方法，Coro无此方法。
- 传统generator不能`yield from`Coro对象，不过被下面协程包装器包装的generator则可以。
- Coro不能抛出`StopIteration`，会被替换成`RuntimeError`异常。
- 若Coro被gc时从未被await过，类比generator初始化后没有通过`send()`开始，会产生`RuntimeError`异常。

```python
async def read_data(db):
    pass
```

提供对generator实现协程的包装器语法，作为历史协程方法向新语法的过度
```python
@types.coroutine
def process_data(db):
    data = yield from read_data(db)
    ...
```


#### await表达式

**`await`表达式**：只能在Coro中使用，在普通函数/生成器中使用是非法的，效果与`yield from`类似。
不同点，`await`要求其等待的对象是 *可等待类型(Awaitable)* 对象，包括:
- `async def`定义的Coro是可等待对象。
- 由包装器`@types.coroutine`包装的传统生成器实现的协程。
- 具有特殊方法`__await__()`的类型，且该返回对象的符合迭代器要求。


#### async with/async for

与`with`语句类似，`async with`提供异步实现的包装器语法。
`with`语句等效调用类型的特殊方法`ctx.__enter__()`与`ctx.__exit__()`。
与之相对`async with`语句等效调用类型的特殊方法`await ctx.__aenter__()`与`await ctx.__aexit__()`

与`for`语句类似，`async for`提供了异步版本的实现。


## 后记

本文从计算机系统的起点开始，沿着并发、协程的线索，
大体上完整回顾了Python语言Coroutine诞生的历史演化过程。

不过，Coroutine不是关于Python协程实现的终点，
如[历史背景与Python协程的诞生](#历史背景与python协程的诞生)章节所说，
仅有协程本身是不够的，协程的并发运行需要相应完善的调度系统支持，
即Python的事件循环——EventLoop。

类似的，本文也只是异步/协程编程开发的起点。_Make every code character full of magic._   