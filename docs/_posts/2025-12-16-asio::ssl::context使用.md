---
title: asio开发：ssl::context如何使用
---


## 一、`asio::ssl::context` 概念

### `ssl::context`功能

`asio::ssl::context`是TLS（Transport Layer Security）配置与共享资源的容器，里面通常包括

- 证书校验配置（verify mode、CA/系统根证书加载）
- 客户端证书/私钥（mTLS 时）
- 协议版本、cipher suites、options
- 会话缓存 / session resumption 相关设置（不同 OpenSSL 版本细节不同，但大方向是“跟 context 相关”）
- 你加载 CA 文件/目录、系统证书等的开销

大多数情况下：复用同一份 ssl::context 更好（更快、更一致、更易维护，还可能更利于 session 恢复）。

只有在 TLS 策略不同（根证书/verify/mTLS/协议栈配置不同）时，才创建多份 ctx（按策略分组），而不是按连接分。

### TLS 策略

TLS 策略本质是说：当作为客户端和服务器建立HTTPS连接是，以什么规则相信对方是真的、是安全的、是客户端期望的服务器。

第一类：与公网 HTTPS 交互的 TLS 策略：

- 服务器出示证书
- 证书必须：由权威CA签发；域名必须匹配；没有过期、失效

```cpp
ctx.set_verify_mode(asio::ssl::verify_peer);
ctx.set_default_verify_paths();  // 使用系统 CA
```

第二类：内网/自签名证书 的 TLS 策略

- 服务器使用：自签名证书 or 公司内部的CA签发证书（系统不认识）

```cpp
ctx.load_verify_file("company_ca.pem");
ctx.set_verify_mode(asio::ssl::verify_peer);

// 不验证，测试使用
ctx.set_verify_mode(asio::ssl::verify_none); 
```

第三类：需要客户端出示身份证 的 TLS 策略（mTLS / 双向TLS）

TLS验证流程：服务器出示证书，客户端验证；客户端出示证书，服务器验证

```cpp
ctx.use_certificate_chain_file("client.crt");
ctx.use_private_key_file("client.key", asio::ssl::context::pem);
ctx.set_verify_mode(asio::ssl::verify_peer);
```


## 二 `ssl::context` 创建与复用

在一个进程中，TLS安全策略一致的情况下，使用一份`ssl::context`可以最大的保障执行效率。

### 局部 static 实例化创建

使用局部static实例化，也能保障多线程并发时安全行。

```cpp
std::shared_ptr<asio::ssl::context> get_default_ssl_ctx()
{
    static std::shared_ptr<asio::ssl::context> ctx = []() {
        auto context = std::make_shared<asio::ssl::context>(asio::ssl::context::sslv23_client);
        context->set_options(
            asio::ssl::context::default_workarounds |
            asio::ssl::context::no_sslv2 |
            asio::ssl::context::no_sslv3 |
            asio::ssl::context::no_tlsv1 |
            asio::ssl::context::no_tlsv1_1
        );
        context->set_default_verify_paths();
        load_root_certificates(*context);
        context->set_verify_mode(asio::ssl::verify_peer);
        return context;
    }();
    return ctx;
}
```

1. 注意到`set_default_verify_paths()`和`load_root_certificates(*context)`可能重复
如果`load_root_certificates`也是把系统/内置根证书再塞一遍，就可能重复加载；
如果它是加载你自带的 PEM（比如 Boost.Beast 示例里那种），那就没问题。

2. `sslv23_client` 含义是“协商最高版本” 它是 OpenSSL 旧命名（并不是 SSLv2/3），
并且`no_sslv2/no_sslv3/no_tlsv1/no_tlsv1_1`，所以最终就是允许 TLS1.2/1.3（取决于 OpenSSL/系统）。这也符合公网客户端。



### 复用`ssl::context`

多份`ssl::context`有什么不利影响

- 性能方面
  - 重复加载/解析 CA 根证书：set_default_verify_paths()、load_verify_file()、你自己的 load_root_certificates() 都可能做文件 IO、解析 PEM、构建 OpenSSL 的证书存储（X509_STORE）
  - 会话复用（session resumption）被割裂：TLS 有“会话恢复/重用”（可以减少握手成本）。很多实现里会话缓存与 ctx（或其内部结构）相关。多份 ctx 可能导致你对同一服务器的连接无法共享这类收益（握手更慢）
  - 更多的OpenSSL内部对象，占用内存
- 行为一致性分析：
  - 相同URL，由于使用了不同的ssl::context，出现不同的行为
- 维护的复杂度更高。总要选择使用哪个ctx$$



## 三、动态库动态加载时 `ssl::context` 的稳妥方案

## 方案A：主程序在插件初始化时注入ssl_ctx，保证全局唯一

主程序提供

```cpp
// host_api.h
#pragma once
#include <boost/asio/ssl/context.hpp>

struct HostServices {
  boost::asio::ssl::context* default_ssl_ctx; // 主程序保证生命周期覆盖所有插件使用期
  // 你还可以顺便塞 io_context、logger、配置、线程池等
};

extern "C" {
  bool plugin_init(const HostServices* host);
  void plugin_shutdown();
}
```

```cpp
HostServices host{};
auto ctx_sp = get_default_ssl_ctx();                 // 你的单例函数放主程序里
host.default_ssl_ctx = ctx_sp.get();                 // 主程序持有 shared_ptr，保证活着

// dlopen + dlsym plugin_init
auto init = (bool(*)(const HostServices*))dlsym(h, "plugin_init");
init(&host);
```

插件程序实现`plugin_init()`，并且获得主程序变量

```cpp
static boost::asio::ssl::context* g_ctx = nullptr;

extern "C" bool plugin_init(const HostServices* host) {
  g_ctx = host->default_ssl_ctx;
  return g_ctx != nullptr;
}

asio::ssl::stream<beast::tcp_stream> make_https_stream(asio::io_context& ioc) {
  return asio::ssl::stream<beast::tcp_stream>(ioc, *g_ctx);
}
```

## 方案B：单独放在共享库中

- 把 `get_default_ssl_ctx()` 的实现放到一个 真正的共享库里，比如 libarena_net.so / libarena_core.so
- 主程序和所有插件都动态链接它（不要静态链接/不要 header-only 复制实现）
- 这样 static 局部变量在该共享库内只会有一份（该 so 在进程里只加载一次）



## 四、主库与插件库使用了不同OpenSSL的问题


1. 插件库`plugin_arena_binance`的测试程序链接了"/opt/homebrew/Cellar/"和"/usr/local/lib/"两个版本OpenSSL

```bash
# 当前程序链接的 ssl
DYLD_PRINT_LIBRARIES=1 ./demo_plugin 2>&1 | grep -E "libssl|libcrypto"

dyld[52058]: <810D99F1-2504-3DB7-9357-4BCE9D47FC76> /opt/homebrew/Cellar/openssl@3/3.6.0/lib/libssl.3.dylib
dyld[52058]: <EFBA4C7A-A8AB-3429-8076-3BF9F22BD40B> /opt/homebrew/Cellar/openssl@3/3.6.0/lib/libcrypto.3.dylib
dyld[52058]: <2CD53B46-B9E0-3697-9959-F53BC135F170> /usr/local/lib/libssl.3.dylib
dyld[52058]: <5FD9128A-F8FF-3F5B-B991-9AFFFBE5E719> /usr/local/lib/libcrypto.3.dylib
dyld[52058]: <987143A3-4E87-30AA-AB95-24B47B1263BF> /usr/lib/libcrypto.46.dylib
dyld[52058]: <96B1B021-485D-3BB6-A4D5-172D1456B736> /usr/lib/libssl.48.dylib
dyld[52058]: move loaded to delayed: libcrypto.46.dylib
dyld[52058]: move loaded to delayed: libssl.48.dylib
    #0 0x100f5123c in ossl_ctrl_internal+0xe0 (libssl.3.dylib:arm64+0x1923c)
SUMMARY: AddressSanitizer: SEGV (libssl.3.dylib:arm64+0x1923c) in ossl_ctrl_internal+0xe0
```

```bash
# demo程序的链接
otool -L ./demo_plugin
./demo_plugin:
	@rpath/libplugin_arena_binance.1.dylib (compatibility version 1.0.0, current version 1.0.0)
	@rpath/libarenad.1.dylib (compatibility version 1.0.0, current version 1.0.0)
	@rpath/libboost_filesystem.dylib (compatibility version 0.0.0, current version 0.0.0)
	@rpath/libboost_atomic.dylib (compatibility version 0.0.0, current version 0.0.0)
	@rpath/libboost_url.dylib (compatibility version 0.0.0, current version 0.0.0)
	/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib (compatibility version 3.0.0, current version 3.0.0)
	/opt/homebrew/opt/openssl@3/lib/libcrypto.3.dylib (compatibility version 3.0.0, current version 3.0.0)
	/opt/homebrew/opt/libsodium/lib/libsodium.26.dylib (compatibility version 29.0.0, current version 29.0.0)
	@rpath/libspdlog.1.12.dylib (compatibility version 1.12.0, current version 1.12.0)
	/usr/lib/libz.1.dylib (compatibility version 1.0.0, current version 1.2.12)
	/usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1800.105.0)
	@rpath/libclang_rt.asan_osx_dynamic.dylib (compatibility version 0.0.0, current version 0.0.0)
	/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1351.0.0)
➜  demo git:(master) ✗ otool -L /opt/arena/lib/libarenad.1.0.0.dylib
```


2. 宿主库`arena`仅链接了"/usr/local/lib"路径的OpenSSL

```bash
# 主库的链接
otool -L /opt/arena/lib/libarenad.1.0.0.dylib
/opt/arena/lib/libarenad.1.0.0.dylib:
	@rpath/libarenad.1.dylib (compatibility version 1.0.0, current version 1.0.0)
	@rpath/libboost_filesystem.dylib (compatibility version 0.0.0, current version 0.0.0)
	@rpath/libboost_url.dylib (compatibility version 0.0.0, current version 0.0.0)
	/usr/local/lib/libssl.3.dylib (compatibility version 3.0.0, current version 3.0.0)
	/usr/local/lib/libcrypto.3.dylib (compatibility version 3.0.0, current version 3.0.0)
	@rpath/libspdlog.1.12.dylib (compatibility version 1.12.0, current version 1.12.0)
	/usr/lib/libz.1.dylib (compatibility version 1.0.0, current version 1.2.12)
	@rpath/libboost_atomic.dylib (compatibility version 0.0.0, current version 0.0.0)
	/opt/homebrew/opt/libsodium/lib/libsodium.26.dylib (compatibility version 29.0.0, current version 29.0.0)
	/usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1800.105.0)
	@rpath/libclang_rt.asan_osx_dynamic.dylib (compatibility version 0.0.0, current version 0.0.0)
	/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1351.0.0)
```

### 问题定位

1. 宿主库"arena"和插件库"plugin_arena_binance"是CMake管理的项目
2. 宿主库"arena"以`PUBLIC`模式`target_link_libraries()`依赖了OpenSSL库
   - CMakeLists中还指定了"OPENSSL_ROOT_DIR"，即OpenSSL的查找路径
3. 插件库以`PUBLIC`模式`target_link_libraries()`依赖了arena。
   - 插件库虽然没有直接链接OpenSSL，但仍需要找到OpenSSL

**解决方案**

- 方案A（一致）：宿主库arena的`arenaConfig.cmake`文件中，保留设置OPENSSL_ROOT_DIR参数。那么插件库就找到相同的OpenSSL库文件
- 方案B（唯一）：调整宿主库arena的代码实现，以`PRIVATE`链接OpenSSL库。
