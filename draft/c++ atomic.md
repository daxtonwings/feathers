## atomic

### 基本读写 load() store()

```c++
#include <iostream>
#include <atomic>
#include <thread>

std::atomic<int> counter(0);

void increment() {
    for (int i = 0; i < 1000; ++i) {
        ++counter;
    }
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);

    t1.join();
    t2.join();

    std::cout << "Final counter value: " << counter << std::endl;
    return 0;
}
```

- `std::atomic<>`类型的读取、赋值方法为`load()`与`store()`，更精确的控制多线程行为
- `store(std::memory_order_release)`确保赋值操作之前的内存数据均写入到内存
- `load(std::memory_order_acquire)`确保读取操作之后，所有读操作都能看到之前的写操作

```c++
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

std::atomic<bool> ready(false);
std::vector<int> data;

void producer() {
    // 生产者准备数据
    data.push_back(1);
    data.push_back(2);
    data.push_back(3);    
    // 确保数据写入内存后，设置 ready 标志
    ready.store(true, std::memory_order_release);
}

void consumer() {
    // 等待生产者准备数据
    while (!ready.load(std::memory_order_acquire)) {
        // busy-wait
    }    
    // 消费者读取数据
    for (int value : data) {
        std::cout << "Data: " << value << std::endl;
    }
}

int main() {
    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);
    producer_thread.join();
    consumer_thread.join();
    return 0;
}
```

std::memory_order_release和std::memory_order_acquire一起使用，
可以确保线程之间的操作顺序，解决多线程环境中常见的数据同步问题。
这种机制在很多并发模式中非常有用，如生产者-消费者、双检查锁定（用于单例模式）、事件通知等。
通过这种方式，可以保证一个线程对共享数据的修改对其他线程是可见的，从而避免数据竞争和不一致的问题。

### compare_exchange_strong() compare_exchange_weak()

```c++
bool std::atomic<T>::compare_exchange_strong(T& expected, 
                             T desired, 
                             std::memory_order success,
                             std::memory_order failure )
```

- 若atomic值与expected一致，则赋新值，返回true
- 若atomic值与expected不一致，返回false，expected更新为atomic

_"expected为atomic在当前线程的缓存"_

compare_exchange_strong()和compare_exchange_weak()是C++中std::atomic类提供的两种比较并交换（CAS）操作。
它们的主要区别在于操作的健壮性和在忙等待算法中的用途。

基本概念
CAS（Compare-And-Swap）：CAS操作用于原子地比较和交换变量的值。
它先比较目标变量与期望值，如果相等则将其替换为新值，否则将目标变量的当前值赋给期望值。

compare_exchange_strong(): 不出现伪失败、性能略差
语义：确保比较和交换操作的强一致性。操作在比较失败时不会出现伪失败（spurious failure）。
用途：适用于不太依赖于性能的场景，确保更高的可靠性。
适用场景：大多数一般的原子操作和非忙等待算法。

compare_exchange_weak(): 可能伪失败、更好性能
语义：允许比较失败时出现伪失败（spurious failure），即使目标变量的值等于期望值，操作也可能返回false。这种伪失败在某些硬件上会频繁出现。
用途：适用于忙等待算法（如自旋锁、自旋等待），这种情况下即使有伪失败也不会对性能产生太大影响，反而有助于优化性能。
适用场景：高性能忙等待算法中，可以容忍伪失败的情况。

### exchange()

exchange函数用于原子地将某个值替换为新值，并返回原来的值。
内存顺序标识

- std::memory_order_acq_rel表示“获取-释放”内存顺序，包含两部分含义：

  - 获取（Acquire）：
  这部分确保在此操作后，所有后续的读操作都能看到在此操作之前对共享数据所做的所有写操作。
  这意味着在调用exchange之后，其他线程进行的所有写入操作都将对当前线程可见。

  - 释放（Release）：
  这部分确保在此操作之前的所有写操作在此操作之前完成。
  当执行exchange时，它会将之前对共享数据的所有写操作刷新到内存中，以确保其他线程在读取数据时看到的是最新的值。

- memory_order_relaxed：不保证内存顺序
- memory_order_consume：之后的读操作能看到之前的写操作
- memory_order_acquire：之后的读操作不会排至此操作前
- memory_order_release：之前的写操作均完成，写入内存
- memory_order_acq_rel：结合了memory_order_acquire+memory_order_release
- memory_order_seq_cst：最高顺序保证，性能效率最低