# exception scaffold

C++ 异常处理演示——try/catch/throw + 自定义异常 + 栈展开 + noexcept + catch-all。

## 复现命令

```bash
cd learning/scaffold/exception

g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
```

## 关键点

- **三段式**：`try { ... } catch (const T& e) { ... }` ——异常类型决定哪个 catch
- **自定义异常**：继承 `std::exception`，覆盖 `what()` 返回 C 字符串
- **栈展开（stack unwinding）**：异常抛出后，所有栈上对象的析构函数按反序调用
- **`noexcept` 关键字**：
  - 声明不抛——编译器可优化（不展开栈）
  - 真抛则 `std::terminate()` 终止进程
  - 移动构造/赋值默认 `noexcept`
- **`catch(...)` 兜底**：捕获一切，但**没有类型信息**
- **标准异常继承树**：`std::exception` → `std::runtime_error` / `std::logic_error`

详见 NOTES.md 工程对照段。