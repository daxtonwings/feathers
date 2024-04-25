---
title: "Python asyncio 协程创建与使用"
---



前文背景：「[异步：从并发到Python协程](https://daxtonwings.github.io/feathers/2024/04/19/from-concurrency-to-python-coroutine.html)」

* **[概念比较](#概念比较)**
    * [什么是并发](#什么是并发)
    * [为什么并发](#为什么要并发)

## 概念比较

- 协程函数（coroutine function），由`async def`定义的函数。
  - 对比1：普通函数是由`def`定义的
  - 对比2：协程函数不是协程对象，协程对象是协程函数的实例。
- 原生协程（coroutine），也是协程对象，异步函数调用（call）的返回对象。
  - 对比：普通函数调用的返回对象就是`return`语句的返回对象
- 生成器（generator），含有`yield`或`yield from`语句的普通函数。
  - 含有`yield`语句的生成器是基础生成器，实现生成器的逆向函数作用。
  - 含有`yield`语句的生成器是代理生成器，代理生成器改善了复杂场景下生成器的使用模式。
- 生成器协程（gen-coroutine），使用`@types.coroutine`装饰的生成器。
- 可等待对象（awaitable）包括
  - 原生协程（coroutine）
  - 生成器协程（gen-coroutine）
  - 定义了特殊方法`__await__()`的类型实例。

|                    | 原生协程协程                  | 基础生成器                 | 代理生成器                | 生成器协程                                  |
|--------------------|-------------------------|-----------------------|----------------------|----------------------------------------|
| 标识语法            | `async def`             | `yield`               | `yield from`       | `@types.coroutine`                     |
| 类型方法`send()`    | 支持                      | 支持                    | 支持                 | 支持                                     |
| 类型方法`thow()`    | 支持                      | 支持                    | 支持                 | 支持                                     |
| 类型方法`close()`   | 支持                      | 支持                    | 支持                 | 支持                                     |
| 类型方法`__iter__()`| --                      | 支持                    | 支持                 | 支持                                     |
| 类型方法`__next__()`| --                      | 支持                    | 支持                 | 支持                                     |
| 暂停点语法           | `r = await [awaitable]` | `r = yield [anyting]` | `r = yield from [generator]` | `r = yield from [generator/awaitable]` |

