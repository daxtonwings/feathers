---
title: "Python asyncio 协程创建与使用"
---

## 背景

「[异步：从并发到Python协程](https://daxtonwings.github.io/feathers/2024/04/19/from-concurrency-to-python-coroutine.html)」
讨论了Python协程（由`async def`定义，下文用Coro表示），
是基于生成器generator语法（`yield`，`yield from`）实现的。
其中`yield`实现基础生成器功能，`yield from`实现代理生成器功能。

|              | Python协程      | 基础生成器       | 代理生成器                |
|--------------|---------------|-------------|----------------------|
| 标识语法         | `async def`   | `yield`     | `yield from`         |
| 类型方法`send()` | 支持            | 支持          | 支持                   |
| 类型方法`thow()`     | 支持            | 支持          | 支持                   |
| 类型方法`close()`    | 支持            | 支持          | 支持                   |
| 类型方法`__iter__()` | --            | 支持          | 支持                   |
| 类型方法`__next__()` | --            | 支持          | 支持                   |
| 暂停点语法        | `r = await awaitable` | `r = yield anyting` | `r = yield from generator` |

