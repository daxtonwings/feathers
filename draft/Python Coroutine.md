
- Python的coroutine是冷启动的，在创建coroutine后必须主动`send(None)`以开始执行

- `yield from`起源，在与generator如何告诉调用者暂停等待的目标