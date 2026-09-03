# python-metaclass scaffold

Python 元类演示——type() + __new__ + ORM 示例 + 注册模式。

## 复现命令

```bash
cd learning/scaffold/python/python-metaclass
python3 main.py
```

## 预期输出

```
[1] type() 动态创建类:
    --- type() 动态创建类 ---
    Point(3,4) 距离原点: 5.00

[2] 自定义元类:
    [Meta] 创建类: MyClass
    [Meta] 基类: ()
    [Meta] 属性数: 3
    MyClass.x = 10

[3] ORM 元类:
    [ORM] 表: users
      - id: INTEGER
      - name: VARCHAR(100)
      - email: VARCHAR(255)
      - age: INTEGER
    [ORM] 表: products
      - id: INTEGER
      - name: VARCHAR(200)
      - price: DECIMAL(10,2)
    User 实例: id=1, name=Alice

[4] 注册表元类:
    [注册] TextPlugin -> <class '__main__.TextPlugin'>
    [注册] ImagePlugin -> <class '__main__.ImagePlugin'>
    注册表: ['Base', 'TextPlugin', 'ImagePlugin']

[5] 元类 + 装饰器:
    打招呼: Bob

=== PASS ===
```

## 关键点

- **type(name, bases, attrs)**：三参数版本动态创建类
- **__new__**：控制类的创建，返回类对象
- **metaclass=**：指定类的元类
- **ORM 场景**：元类自动收集字段，生成表结构

详见 NOTES.md。
