import asyncio
import types


class Foo:
    def __init__(self):
        self._val = 0
        self._loop = asyncio.get_running_loop()

        self._callbacks = []

    def future_call(self):
        self._val = 1

        for callback, ctx in self._callbacks:
            self._loop.call_soon(callback, self)

    def add_done_callback(self, fn, *, context=None):
        """asyncio Task interface"""
        self._callbacks.append((fn, context))

    def result(self):
        """asyncio Task interface"""
        return self._val

    @types.coroutine
    def awaiter(self):
        loop = asyncio.get_running_loop()
        loop.call_later(3, self.future_call)

        self._asyncio_future_blocking = True
        self._loop = loop

        yield self
        return self._val


async def main():
    coroutine = Foo()
    r = await coroutine.awaiter()
    print(r)


asyncio.run(main(), debug=True)
