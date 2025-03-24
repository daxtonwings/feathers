
## co_spawn()

使用`asio::use_future`

```cpp
#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/use_future.hpp>
#include <iostream>
#include <future>

asio::awaitable<int> example_coroutine()
{
    auto executor = co_await asio::this_coro::executor;
    std::cout << "Starting coroutine" << std::endl;

    // 模拟异步操作
    asio::steady_timer timer(executor, std::chrono::seconds(1));
    co_await timer.async_wait(asio::use_awaitable);

    std::cout << "Coroutine completed" << std::endl;
    co_return 42;  // 返回一个整数
}

int main()
{
    asio::io_context io_context;

    // 使用 co_spawn 启动协程，并获取 future
    std::future<int> result = asio::co_spawn(io_context, example_coroutine(), asio::use_future);

    // 运行 io_context
    io_context.run();

    // 获取协程的返回值并处理可能的异常
    try
    {
        int value = result.get();
        std::cout << "Coroutine returned: " << value << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Coroutine failed with exception: " << e.what() << std::endl;
    }

    return 0;
}
```


使用自定义的处理程序，类似`add_done_callback()`

```cpp
#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <iostream>

asio::awaitable<int> example_coroutine()
{
    auto executor = co_await asio::this_coro::executor;
    std::cout << "Starting coroutine" << std::endl;

    // 模拟异步操作
    asio::steady_timer timer(executor, std::chrono::seconds(1));
    co_await timer.async_wait(asio::use_awaitable);

    std::cout << "Coroutine completed" << std::endl;
    co_return 42;  // 返回一个整数
}

void coroutine_handler(std::exception_ptr e, int result)
{
    if (e)
    {
        try
        {
            std::rethrow_exception(e);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Coroutine failed with exception: " << ex.what() << std::endl;
            return;
        }
    }

    std::cout << "Coroutine returned: " << result << std::endl;
}

int main()
{
    asio::io_context io_context;

    // 使用 co_spawn 启动协程，并指定自定义完成处理程序
    asio::co_spawn(io_context, example_coroutine(), coroutine_handler);

    // 运行 io_context
    io_context.run();

    return 0;
}
```

### when_all() 类似asyncio.gather()

```cpp
asio::awaitable<void> main_coroutine()
{
    auto executor = co_await asio::this_coro::executor;

    auto [result1, result2] = co_await asio::when_all(
        coroutine1(),
        coroutine2()
        // 其他协程
    );
}
```

### 使用futures功能等待首个协程完成

```cpp
asio::awaitable<void> main_coroutine()
{
    auto executor = co_await asio::this_coro::executor;

    // 启动多个协程
    std::vector<asio::awaitable<int>> coroutines = {
        coroutine(1, 3),  // 3秒完成
        coroutine(2, 1),  // 1秒完成
        coroutine(3, 2)   // 2秒完成
    };

    // 启动协程并保存它们的任务
    std::vector<std::future<int>> futures;
    for (auto& coro : coroutines)
    {
        futures.push_back(asio::co_spawn(executor, std::move(coro), asio::use_future));
    }

    // 等待第一个完成的协程
    for (auto& future : futures)
    {
        try
        {
            int result = future.get();  // 阻塞等待，直到某个协程完成
            std::cout << "First completed coroutine returned: " << result << std::endl;
            break; // 找到第一个完成的协程后退出循环
        }
        catch (const std::future_error& e)
        {
            std::cout << "Coroutine was cancelled: " << e.what() << std::endl;
        }
    }

    // 取消其他协程逻辑可以放在这里
}

```