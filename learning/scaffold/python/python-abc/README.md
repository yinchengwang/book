# python-abc scaffold

Python ABC 抽象基类演示——ABC + abstractmethod + register。

## 复现命令

```bash
cd learning/scaffold/python/python-abc
python3 main.py
```

## 预期输出

```
[1] 抽象基类:
    狗: speak=汪汪！, move=奔跑
    猫: speak=喵~, move=跳跃

[2] 抽象属性:
    矩形(3x4): area=12.00, perimeter=14.00
    圆(r=2): area=12.57, perimeter=12.57

[3] register 虚拟子类:
    Panther 是 Animal 的虚拟子类: True

=== PASS ===
```

## 关键点

- **ABC**：抽象基类基类
- **@abstractmethod**：抽象方法，派生类必须实现
- **抽象属性**：@property + @abstractmethod
- **register()**：注册虚拟子类

详见 NOTES.md。
