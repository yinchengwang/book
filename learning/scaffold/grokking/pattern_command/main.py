"""
命令模式 (Command Pattern) - 文本编辑器示例
将请求封装为对象，支持参数化、排队、撤销/重做。
"""
from abc import ABC, abstractmethod


class Command(ABC):
    """命令抽象基类"""
    @abstractmethod
    def execute(self) -> None:
        pass

    @abstractmethod
    def undo(self) -> None:
        pass


class TextEditor:
    """接收者 —— 文本编辑器"""

    def __init__(self) -> None:
        self._content: list[str] = []

    def insert(self, text: str, pos: int | None = None) -> None:
        if pos is None:
            self._content.append(text)
        else:
            self._content.insert(pos, text)

    def delete(self, length: int, pos: int | None = None) -> str:
        if pos is None:
            pos = len(self._content) - length if length <= len(self._content) else 0
        deleted = "".join(self._content[pos:pos + length])
        self._content[pos:pos + length] = []
        return deleted

    @property
    def content(self) -> str:
        return "".join(self._content)

    def __str__(self) -> str:
        return f'TextEditor("{self.content}")'


class InsertCommand(Command):
    """插入命令"""

    def __init__(self, editor: TextEditor, text: str, pos: int | None = None) -> None:
        self.editor = editor
        self.text = text
        self.pos = pos

    def execute(self) -> None:
        self.editor.insert(self.text, self.pos)

    def undo(self) -> None:
        self.editor.delete(len(self.text), self.pos)


class DeleteCommand(Command):
    """删除命令"""

    def __init__(self, editor: TextEditor, length: int, pos: int | None = None) -> None:
        self.editor = editor
        self.length = length
        self.pos = pos
        self._saved: str = ""

    def execute(self) -> None:
        self._saved = self.editor.delete(self.length, self.pos)

    def undo(self) -> None:
        self.editor.insert(self._saved, self.pos)


class CommandHistory:
    """命令历史 —— 管理撤销/重做栈"""

    def __init__(self) -> None:
        self._undo_stack: list[Command] = []
        self._redo_stack: list[Command] = []

    def execute(self, cmd: Command) -> None:
        cmd.execute()
        self._undo_stack.append(cmd)
        self._redo_stack.clear()

    def undo(self) -> None:
        if not self._undo_stack:
            return
        cmd = self._undo_stack.pop()
        cmd.undo()
        self._redo_stack.append(cmd)

    def redo(self) -> None:
        if not self._redo_stack:
            return
        cmd = self._redo_stack.pop()
        cmd.execute()
        self._undo_stack.append(cmd)


def main() -> None:
    editor = TextEditor()
    history = CommandHistory()

    print("=== 命令模式演示 ===")
    print(f"初始: {editor}")

    # 输入 "Hello World!"
    for ch in "Hello World!":
        history.execute(InsertCommand(editor, ch))
    print(f"输入后: {editor}")

    # 撤销 " World!"（7 个字符 —— " World!" 包含空格和感叹号）
    for _ in range(7):
        history.undo()
    print(f"撤销 ' World!' 后: {editor}")

    # 重做回去
    for _ in range(7):
        history.redo()
    print(f"重做后: {editor}")

    # 全部撤销
    for _ in range(12):
        history.undo()
    print(f"撤销全部后: {editor}")

    # 再次输入
    for ch in "Hello World!":
        history.execute(InsertCommand(editor, ch))
    print(f"重新输入: {editor}")


if __name__ == "__main__":
    main()
