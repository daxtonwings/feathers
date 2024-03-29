---
title:  "Python异步（二） asyncio EventLoop实现"
---



## 一、I/O 并发概述

Unix系统的`select()`,`poll()`,`epoll()`,`kqueue()`的系统调用开启了I/O多路复用的时代，使上层应用代码在避免等待I/O时的资源消耗，从而使并发大量I/O操作操作效率提高。对比原始的I/O轮询模式，开始多路I/O操作后需要不断循环查询I/O操作状态，操作效率低下。即使使用多线程并发处理不同的I/O操作，仍无法等待I/O操作结构时的轮询资源消耗。

在`select/epoll`系统框架上，为每个I/O操作指定后续的callback回调方法，就可以消除上层应用的轮询等待。所有上层代码将无阻塞的并发执行。

I/O并发则是不断新增I/O操作、对I/O操作结果执行callback的顺序动作。但由于I/O操作结果返回的时间是不可预测的，因此要有适当的调度中心对callback动作进行管理。

Python `asyncio`内部的EventLoop机制就是管理callback的调度模型，而且扩展了callback对象范围，不只对狭义的I/O操作结果处理，也对广义的生成器(generator)对象实现调度管理。



## 二、asyncio EventLoop

> [PEP-3156](https://peps.python.org/pep-3156/)

### 2.1) EventLoop 框架研究

传统的直线阻塞式I/O操作遵循的步骤是：（1）action: 执行I/O操作->（2）waiting: 循环等待I/O响应结果->（3）callback: 处理结果与后续逻辑。优点是：执行逻辑连贯，I/O操作上下文状态一致。缺点是等待结果的步骤（2）造成程序阻塞。

异步I/O操作依托操作系统的`select/poll`将阻塞步骤（2）交由系统层面处理，应用程序只处理（1）（3）部分逻辑。缺点是：（1）（3）步骤执行逻辑是分离的（即称为“异步I/O”的原因）。

因此，异步I/O框架的实现，重点实现如下需求：

1. 依托操作系统的`select/poll`方法，应用程序不需阻塞等待I/O操作结果
2. 应用程序程序做好（1）action与（3）callback的关联动作。即开始I/O操作后，要为`select/poll`可能产生的I/O结果做好callback指定

`EventLoop`是位于操作系统`select/poll`与上层应用程序`action/callback`的中间层

- 对于操作系统，`EventLoop`建立I/O，并接收`select/poll`的I/O响应
- 对于应用程序，`EventLoop`接收应用程序的I/O请求，并调动执行callback



### 2.2）EventLoop 方法概述

- 启动(starting), 停止(stopping), 关闭(closing): 
  - `run_forever()`
  - `run_until_complete()`
  - `stop()` 停止的Loop可以再次启动，关闭的Loop不能再启动
  - `is_running()`
  - `is_closed()`
  - `close()`

- 添加callback的方法: 
  - `call_soon()` -> 返回类型**Handle**
  - `call_later()` -> 返回类型**TimerHandle**
  - `call_at()` -> 返回类型**TimerHandle**
  - `time()`

- Future与Task:
  - `create_future()` -> 返回**Future**类型(更丰富的Handle)
    - Future对象抽象指代了某种I/O操作的未来结果 + 相关的callback方法
    - I/O结果具体完成时间未知，来源于操作系统信息
    - callback由Future对象得到I/O操作结果后再向EventLoop添加执行
  - `create_task()` -> 返回**Task**类型
    - Task是功能更丰富的Future对象
  - `set_task_factory()`
  - `get_task_factory()`

- 线程交互方法: 

  - `call_soon_threadsafe()`
  - `run_in_executor()`
  - `set_default_executor()`

- Network I/O域名解析方法（异步方法）: 

  - `async getaddrinfo()`
  - `async getnameinfo()`

- Network I/O连接方法（异步方法）: 
  - `async create_connection()`
  - `async create_server()` -> 返回类型**AbstractServer**
  - `async create_datagram_endpoint()`
  - 其它补充：
    - `async sendfile()`
    - `async start_tls()`
    - `async create_unix_connection()`
    - `async create_unix_server()`

- socket封装方法（异步方法）: 
  - `async sock_recv()`(`async sock_recv_into()`)
  - `async sock_sendall()`
  - `async sock_connect()`
  - `async sock_accept()`
  - `async sock_sendfile()`

- 异常处理: 

  - `get_exception_handler()`
  - `set_exception_handler()`
  - `default_exception_handler()`
  - `call_exception_handler()`

- Debug模式: 

  - `get_debug()`
  - `set_debug()`

  

**部分支持功能, UNIX支持**

- I/O 补充操作: 
  - `add_reader()`
  - `remove_reader()`
  - `add_writer()`
  - `remove_writer()`
- 信号处理: 
  - `add_signal_handler()`
  - `remove_signal_handler()`
- 管道与子进程: 
  - `async connect_read_pipe()`
  - `async connect_write_pipe()`
  - `async subprocess_shell()`, 
  - `async subprocess_exec()`

### 2.3）Handle, TimerHandle表示已添加到EventLoop上的callback

**Handle**

1. `Handle(callback, args, loop, context=None)`, 属性
   - `_context`
   - `_loop`
   - `_callback`
   - `_args`
   - `_cancelled`
   - `_source_traceback`: 在debug模式下记录调用栈信息
2. 方法
   - `cancel()`
   - `canceled()`
   - `_run()`: 供EventLoop使用的内部方法
   

**TimerHandle(Handle)**

1. `TimerHandle(when, callback, args, loop, context=None)`
   - 额外的`_when`
   - 额外的`_scheduled`
2. 补充方法
   - `when()`计划执行时间


## 三、实现代码研究

### 3.1）BaseEventLoop(AbstractEventLoop)

#### 运行循环 run_forever()

```python
class BaseEventLoop:
    def run_forever(self):
        """Run until stop() is called."""
        self._check_closed()
        self._check_running()
        self._set_coroutine_origin_tracking(self._debug)
        self._thread_id = threading.get_ident()

        # 为当前loop设置异步生成器的firstiter, finalizer, python异步迭代器完备性要求
        old_agen_hooks = sys.get_asyncgen_hooks()
        sys.set_asyncgen_hooks(firstiter=self._asyncgen_firstiter_hook,
                               finalizer=self._asyncgen_finalizer_hook)
        try:
            events._set_running_loop(self)
            while True:
                # 循环执行
                self._run_once()
                if self._stopping:
                    break
        finally:
            self._stopping = False
            self._thread_id = None
            events._set_running_loop(None)
            self._set_coroutine_origin_tracking(False)
            sys.set_asyncgen_hooks(*old_agen_hooks)
```

#### 单步动作执行

```python
class BaseEventLoop:
    def _run_once(self):
        # 清理定时任务(堆)
        # 1. 定时任务取消数量>50%, 且大于>50个时, 清理重建堆
        # 2. 清理堆顶已取消的任务
        sched_count = len(self._scheduled)
        if (sched_count > _MIN_SCHEDULED_TIMER_HANDLES and
            self._timer_cancelled_count / sched_count >
                _MIN_CANCELLED_TIMER_HANDLES_FRACTION):
            new_scheduled = []
            for handle in self._scheduled:
                if handle._cancelled:
                    handle._scheduled = False
                else:
                    new_scheduled.append(handle)

            heapq.heapify(new_scheduled)
            self._scheduled = new_scheduled
            self._timer_cancelled_count = 0
        else:
            while self._scheduled and self._scheduled[0]._cancelled:
                self._timer_cancelled_count -= 1
                handle = heapq.heappop(self._scheduled)
                handle._scheduled = False

        # ready是已完成的非定时任务队列
        timeout = None
        if self._ready or self._stopping:
            timeout = 0
        elif self._scheduled:
            # 依据定时任务计算合适的timeout时长, 不小于0, 不大于24*3600
            when = self._scheduled[0]._when
            timeout = min(max(0, when - self.time()), MAXIMUM_SELECT_TIMEOUT)

        # _selector由loop的具体实现定义, select
        # _process_events同样留给loop的具体实现定义
        event_list = self._selector.select(timeout)
        # select得到的event_list向self._ready添加了相应的callback
        self._process_events(event_list)

        # Handle 'later' callbacks that are ready.
        end_time = self.time() + self._clock_resolution
        while self._scheduled:
            handle = self._scheduled[0]
            if handle._when >= end_time:
                break
            handle = heapq.heappop(self._scheduled)
            handle._scheduled = False
            self._ready.append(handle)

        # ～～此处是callback被真正调用的部分～～.
        ntodo = len(self._ready)
        for i in range(ntodo):
            handle = self._ready.popleft()
            if handle._cancelled:
                continue
            if self._debug:
                try:
                    self._current_handle = handle
                    t0 = self.time()
                    # ～～调用callback～～, 所有callback都是Handle的形式
                    handle._run()
                    dt = self.time() - t0
                    if dt >= self.slow_callback_duration:
                        logger.warning('Executing %s took %.3f seconds',
                                        _format_handle(handle), dt)
                finally:
                    self._current_handle = None
            else:
                # ～～调用callback～～, 所有callback都是Handle的形式
                handle._run()
        handle = None  # Needed to break cycles when an exception occurs.
```

EventLoop每次步进循环中，检查可以执行的callback的Handle对象，来源包括：

1. `call_soon()`直接安排的callback, `Handle`
   - 由`self._ready`维护，无额外等待，再下次EventLoop步进中按先后顺序执行
1. `call_at()`,`call_later()`安排的相对时间callback, `TimerHandle`
   - 由`self._scheduled`列表维护
   - 相对时间基于EventLoop方法`time()`进行判断。（无需系统select/poll）
3. 由`self._selector`通过`select/poll`方法检查的已就绪的I/O对应的callback
   - 可能被安排的callback通过SelectorEventLoop的`add_reader(self, fd, callback, *args)`与`add_writer(self, fd, callback, *args)`方法进行安排
   - 在文件描述符`fd`每次读/写就绪时，利用`_add_callback()`安排对于`Handle`的回调



#### 添加callback

call_soon(), call_later(), call_at()

```python
class BaseEventLoop:
    def _call_soon(self, callback, args, context):
        # 为callback创建Handle
        handle = events.Handle(callback, args, self, context)
        ...
        # 将callback添加至ready队列
        self._ready.append(handle)
        return handle

    def call_soon(self, callback, *args, context=None):
        self._check_closed()
        ...
        handle = self._call_soon(callback, args, context)
        ...
        return handle

    def call_at(self, when, callback, *args, context=None):
        self._check_closed()
        ...
        timer = events.TimerHandle(when, callback, args, self, context)
        ...
        # 将定时任务的handle添加到scheduled队列
        heapq.heappush(self._scheduled, timer)
        timer._scheduled = True
        return timer
      
    def _add_callback(self, handle):
        """Add a Handle to _scheduled (TimerHandle) or _ready."""
        assert isinstance(handle, events.Handle), 'A Handle is required here'
        if handle._cancelled:
            return
        assert not isinstance(handle, events.TimerHandle)
        self._ready.append(handle)
```



### 3.2) SelectorEventLoop(BaseEventLoop)

**SelectorEventLoop对于`self._selector`的实现**

```python
# 处理事件方法
class BaseSelectorEventLoop(BaseEventLoop):
    def __init__(self)
        # _selector具体实现
        self._selector = selectors.SelectSelector()
        ...

    def _process_events(self, event_list):
        for key, mask in event_list:
            # key是SelectorKey对象, key.data是非透明的自定义数据
            fileobj, (reader, writer) = key.fileobj, key.data
            if mask & selectors.EVENT_READ and reader is not None:
                if reader._cancelled:
                    self._remove_reader(fileobj)
                else:
                    # 如果文件对象的读就绪, 那么调用一次reader的callback
                    self._add_callback(reader)
            if mask & selectors.EVENT_WRITE and writer is not None:
                if writer._cancelled:
                    self._remove_writer(fileobj)
                else:
                    self._add_callback(writer)
                    
    def _add_reader(self, fd, callback, *args):
        self._check_closed()
        handle = events.Handle(callback, args, self, None)
        try:
            key = self._selector.get_key(fd)
        except KeyError:
            self._selector.register(fd, selectors.EVENT_READ,
                                    (handle, None))
        else:
            mask, (reader, writer) = key.events, key.data
            self._selector.modify(fd, mask | selectors.EVENT_READ,
                                  (handle, writer))
            if reader is not None:
                reader.cancel()
        return handle

    def _remove_reader(self, fd):
        if self.is_closed():
            return False
        try:
            key = self._selector.get_key(fd)
        except KeyError:
            return False
        else:
            mask, (reader, writer) = key.events, key.data
            mask &= ~selectors.EVENT_READ
            if not mask:
                self._selector.unregister(fd)
            else:
                self._selector.modify(fd, mask, (None, writer))

            if reader is not None:
                reader.cancel()
                return True
            else:
                return False

    def _add_writer(self, fd, callback, *args):
        self._check_closed()
        handle = events.Handle(callback, args, self, None)
        try:
            key = self._selector.get_key(fd)
        except KeyError:
            self._selector.register(fd, selectors.EVENT_WRITE,
                                    (None, handle))
        else:
            mask, (reader, writer) = key.events, key.data
            self._selector.modify(fd, mask | selectors.EVENT_WRITE,
                                  (reader, handle))
            if writer is not None:
                writer.cancel()
        return handle

    def _remove_writer(self, fd):
        """Remove a writer callback."""
        if self.is_closed():
            return False
        try:
            key = self._selector.get_key(fd)
        except KeyError:
            return False
        else:
            mask, (reader, writer) = key.events, key.data
            # Remove both writer and connector.
            mask &= ~selectors.EVENT_WRITE
            if not mask:
                self._selector.unregister(fd)
            else:
                self._selector.modify(fd, mask, (reader, None))

            if writer is not None:
                writer.cancel()
                return True
            else:
                return False

    def add_reader(self, fd, callback, *args):
        """Add a reader callback."""
        self._ensure_fd_no_transport(fd)
        self._add_reader(fd, callback, *args)

    def add_writer(self, fd, callback, *args):
        """Add a writer callback.."""
        self._ensure_fd_no_transport(fd)
        self._add_writer(fd, callback, *args)
```

