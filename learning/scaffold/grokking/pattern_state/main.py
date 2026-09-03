"""
状态模式 (State Pattern) 示例

对象内部状态变化时改变其行为, 消除 if-else/switch 分支判断。
"""

import abc
import time
from enum import Enum


# ─── 状态接口 ─────────────────────────────────────────────────
class ConnectionState(abc.ABC):
    """TCP 连接状态的抽象接口"""

    @abc.abstractmethod
    def open(self, conn: "TCPConnection") -> None:
        """尝试打开连接"""
        raise NotImplementedError

    @abc.abstractmethod
    def close(self, conn: "TCPConnection") -> None:
        """尝试关闭连接"""
        raise NotImplementedError

    @abc.abstractmethod
    def send(self, conn: "TCPConnection", data: str) -> None:
        """发送数据"""
        raise NotImplementedError

    @abc.abstractmethod
    def receive(self, conn: "TCPConnection") -> str:
        """接收数据"""
        raise NotImplementedError

    @abc.abstractmethod
    def status(self) -> str:
        """返回当前状态描述"""
        raise NotImplementedError


# ─── 具体状态: CLOSED ───────────────────────────────────────
class ClosedState(ConnectionState):
    def open(self, conn: "TCPConnection") -> None:
        print("[CLOSED] 连接已打开, 进入 ESTABLISHED 状态")
        conn._state = EstablishedState()

    def close(self, conn: "TCPConnection") -> None:
        print("[CLOSED] 连接已经处于关闭状态, 无操作")

    def send(self, conn: "TCPConnection", data: str) -> None:
        print("[CLOSED] 错误: 连接未打开, 无法发送数据")
        raise RuntimeError("Connection not established")

    def receive(self, conn: "TCPConnection") -> str:
        print("[CLOSED] 错误: 连接未打开, 无法接收数据")
        raise RuntimeError("Connection not established")

    def status(self) -> str:
        return "CLOSED"


# ─── 具体状态: ESTABLISHED ──────────────────────────────────
class EstablishedState(ConnectionState):
    def open(self, conn: "TCPConnection") -> None:
        print("[ESTABLISHED] 连接已经建立, 无操作")

    def close(self, conn: "TCPConnection") -> None:
        print("[ESTABLISHED] 关闭连接, 进入 CLOSED 状态")
        conn._state = ClosedState()

    def send(self, conn: "TCPConnection", data: str) -> None:
        print(f"[ESTABLISHED] 发送数据: {data!r}")
        conn._buffer.append(data)
        print(f"[ESTABLISHED] 缓冲区数据量: {len(conn._buffer)} 条")

    def receive(self, conn: "TCPConnection") -> str:
        if conn._buffer:
            data = conn._buffer.pop(0)
            print(f"[ESTABLISHED] 接收数据: {data!r}")
            return data
        print("[ESTABLISHED] 缓冲区为空, 无数据可接收")
        return ""

    def status(self) -> str:
        return "ESTABLISHED"


# ─── 具体状态: LISTENING (额外展示) ─────────────────────────
class ListeningState(ConnectionState):
    """服务器监听状态, 可接受新连接"""

    def open(self, conn: "TCPConnection") -> None:
        print("[LISTENING] 接受新连接, 进入 ESTABLISHED 状态")
        conn._state = EstablishedState()

    def close(self, conn: "TCPConnection") -> None:
        print("[LISTENING] 停止监听, 进入 CLOSED 状态")
        conn._state = ClosedState()

    def send(self, conn: "TCPConnection", data: str) -> None:
        print("[LISTENING] 错误: 监听中无法发送数据")
        raise RuntimeError("Cannot send while listening")

    def receive(self, conn: "TCPConnection") -> str:
        print("[LISTENING] 监听中, 等待新连接...")
        return ""

    def status(self) -> str:
        return "LISTENING"


# ─── 上下文: TCP Connection ─────────────────────────────────
class TCPConnection:
    """TCP 连接上下文, 维护当前状态"""

    def __init__(self, initial_state: ConnectionState | None = None):
        # 缓冲队列
        self._buffer: list[str] = []
        # 连接标识
        self._id = id(self)
        # 初始状态 (默认 CLOSED)
        self._state = initial_state if initial_state is not None else ClosedState()
        print(f"[TCP #{self._id:x}] 初始状态: {self._state.status()}")

    # ── 状态委派方法 ──
    def open(self) -> None:
        self._state.open(self)

    def close(self) -> None:
        self._state.close(self)

    def send(self, data: str) -> None:
        self._state.send(self, data)

    def receive(self) -> str:
        return self._state.receive(self)

    @property
    def state(self) -> str:
        return self._state.status()

    # 允许外部切换状态 (用于高级场景)
    def set_state(self, state: ConnectionState) -> None:
        old = self._state.status()
        self._state = state
        print(f"[TCP #{self._id:x}] 状态切换: {old} -> {self._state.status()}")


# ─── 额外演示: 状态表驱动的 FSM ─────────────────────────────
class LightState(Enum):
    OFF = "off"
    LOW = "low"
    MEDIUM = "medium"
    HIGH = "high"


# 状态转换表: (当前状态, 事件) -> (下一状态, 动作)
LIGHT_TRANSITIONS = {
    (LightState.OFF, "toggle"): (LightState.LOW, "灯光亮度: 低"),
    (LightState.LOW, "toggle"): (LightState.MEDIUM, "灯光亮度: 中"),
    (LightState.MEDIUM, "toggle"): (LightState.HIGH, "灯光亮度: 高"),
    (LightState.HIGH, "toggle"): (LightState.OFF, "灯光关闭"),
    # 长按直接关闭
    (LightState.LOW, "long_press"): (LightState.OFF, "灯光关闭 (长按)"),
    (LightState.MEDIUM, "long_press"): (LightState.OFF, "灯光关闭 (长按)"),
    (LightState.HIGH, "long_press"): (LightState.OFF, "灯光关闭 (长按)"),
    (LightState.OFF, "long_press"): (LightState.OFF, "已经关闭"),
}


class LightSwitch:
    """基于状态表驱动的灯开关 FSM"""

    def __init__(self):
        self._current = LightState.OFF

    def handle_event(self, event: str) -> str:
        key = (self._current, event)
        if key not in LIGHT_TRANSITIONS:
            return f"事件 {event!r} 在状态 {self._current.value} 下不被支持"
        next_state, action = LIGHT_TRANSITIONS[key]
        self._current = next_state
        return action

    @property
    def current(self) -> str:
        return self._current.value


# ─── 主函数 ─────────────────────────────────────────────────
def main():
    print("=" * 56)
    print("状态模式 演示 - TCP 连接状态机")
    print("=" * 56)

    conn = TCPConnection()

    # 1. CLOSED 状态下发送数据 -> 异常
    print("\n--- 1. 尝试在 CLOSED 状态下发送数据 ---")
    try:
        conn.send("hello")
    except RuntimeError as e:
        print(f"  捕获异常: {e}")

    # 2. 打开连接: CLOSED -> ESTABLISHED
    print("\n--- 2. 打开连接 ---")
    conn.open()
    assert conn.state == "ESTABLISHED"
    print(f"  当前状态: {conn.state}")

    # 3. ESTABLISHED 下收发数据
    print("\n--- 3. 收发数据 ---")
    conn.send("Hello, World!")
    conn.send("第二条消息")
    received = conn.receive()
    assert received == "Hello, World!"
    received = conn.receive()
    assert received == "第二条消息"

    # 4. 关闭连接: ESTABLISHED -> CLOSED
    print("\n--- 4. 关闭连接 ---")
    conn.close()
    assert conn.state == "CLOSED"
    print(f"  当前状态: {conn.state}")

    # 5. 再次建立连接并发送数据
    print("\n--- 5. 重新建立连接 ---")
    conn.open()
    conn.send("再次连接后的数据")
    conn.close()

    print("\n" + "=" * 56)
    print("状态表驱动 FSM 演示 - 灯开关")
    print("=" * 56)

    light = LightSwitch()

    for i in range(6):
        print(f"  当前: {light.current:>8} | toggle -> {light.handle_event('toggle')}")
    print(f"  当前: {light.current:>8} | long_press -> {light.handle_event('long_press')}")

    print("\n" + "=" * 56)
    print("状态模式核心要点")
    print("=" * 56)
    print("  * 状态接口: 每个状态实现相同的接口方法")
    print("  * 状态转换: 由状态类自身决定下一个状态")
    print("  * 消除分支: 用多态替代 if-else/switch")
    print("  * 状态表: 对于简单场景, 状态表驱动更实用")
    print("=" * 56)


if __name__ == "__main__":
    main()
