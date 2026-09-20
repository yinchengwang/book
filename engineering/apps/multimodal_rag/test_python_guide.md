# Python编程入门指南

## 第一章：Python基础

Python是一种高级编程语言，由Guido van Rossum于1991年首次发布。Python的设计哲学强调代码的可读性和简洁性。

### 1.1 变量和数据类型

Python支持多种内置数据类型：

1. **int（整数）**：如 42, -7, 0
2. **float（浮点数）**：如 3.14, -2.5
3. **str（字符串）**：如 "hello", "world"
4. **bool（布尔值）**：True 或 False
5. **list（列表）**：如 [1, 2, 3]，可变序列
6. **dict（字典）**：如 {"name": "Alice", "age": 30}，键值对集合

### 1.2 控制流

Python使用if-elif-else进行条件判断：

```python
score = 85
if score >= 90:
    grade = "A"
elif score >= 80:
    grade = "B"
elif score >= 70:
    grade = "C"
else:
    grade = "D"
```

### 1.3 函数定义

使用def关键字定义函数：

```python
def calculate_average(numbers):
    """计算数字列表的平均值"""
    if not numbers:
        return 0
    total = sum(numbers)
    return total / len(numbers)
```

## 第二章：面向对象编程

Python完全支持面向对象编程（OOP）。

### 2.1 类和对象

```python
class Animal:
    def __init__(self, name, species):
        self.name = name
        self.species = species
    
    def speak(self):
        return f"{self.name} makes a sound"
```

### 2.2 继承

子类可以继承父类的属性和方法：

```python
class Dog(Animal):
    def __init__(self, name, breed):
        super().__init__(name, species="Canine")
        self.breed = breed
    
    def speak(self):
        return f"{self.name} says Woof!"
```

## 第三章：标准库

Python拥有强大的标准库：

- **os**：操作系统接口
- **json**：JSON解析
- **datetime**：日期和时间处理
- **re**：正则表达式
- **collections**：高级数据结构

## 总结

Python是一种功能强大且易于学习的编程语言，适用于Web开发、数据分析、人工智能、自动化脚本等多种场景。
