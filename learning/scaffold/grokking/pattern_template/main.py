"""
模板方法模式 (Template Method Pattern) 示例

在父类中定义算法骨架, 子类实现具体步骤。
本示例展示: 数据迁移框架 + 饮料制作 + 测试用例框架。
"""

import abc
import time
import os
import tempfile
from typing import final


# =================================================================
# 示例 1: 数据迁移框架
# =================================================================

class DataMigration(abc.ABC):
    """
    数据迁移模板方法框架。

    算法骨架:
    1. 连接源数据源
    2. 抽取数据
    3. 转换数据 (可选的钩子)
    4. 加载到目标
    5. 清理
    """

    def migrate(self) -> None:
        """模板方法: 定义迁移流程骨架, 子类不应覆写"""
        self._connect_source()
        data = self._extract()
        transformed = self._transform(data)  # 钩子方法
        self._load(transformed)
        self._cleanup_hook()  # 钩子方法
        self._disconnect_all()
        print(f"[{self.__class__.__name__}] 迁移完成!")

    # ── 抽象方法 (子类必须实现) ──
    @abc.abstractmethod
    def _connect_source(self) -> None:
        """连接源数据源"""
        ...

    @abc.abstractmethod
    def _extract(self) -> list[dict]:
        """从源抽取数据"""
        ...

    @abc.abstractmethod
    def _load(self, data: list[dict]) -> None:
        """加载到目标"""
        ...

    @abc.abstractmethod
    def _disconnect_all(self) -> None:
        """断开所有连接"""
        ...

    # ── 钩子方法 (子类可选覆写) ──
    def _transform(self, data: list[dict]) -> list[dict]:
        """数据转换钩子, 默认不做转换"""
        return data

    def _cleanup_hook(self) -> None:
        """清理钩子, 默认无操作"""
        pass


class CSVToJSONMigration(DataMigration):
    """CSV -> JSON 文件迁移"""

    def __init__(self, csv_path: str, json_path: str):
        self._csv_path = csv_path
        self._json_path = json_path
        self._csv_file = None
        self._lines: list[str] = []

    def _connect_source(self) -> None:
        print(f"  打开 CSV 文件: {self._csv_path}")
        with open(self._csv_path, "r") as f:
            self._lines = f.readlines()

    def _extract(self) -> list[dict]:
        print("  解析 CSV 数据...")
        if not self._lines:
            return []
        headers = [h.strip() for h in self._lines[0].strip().split(",")]
        records: list[dict] = []
        for line in self._lines[1:]:
            values = [v.strip() for v in line.strip().split(",")]
            if len(values) == len(headers):
                records.append(dict(zip(headers, values)))
        print(f"  抽取 {len(records)} 条记录")
        return records

    def _load(self, data: list[dict]) -> None:
        print(f"  写入 JSON 文件: {self._json_path}")
        import json
        with open(self._json_path, "w") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        print(f"  已写入 {len(data)} 条 JSON 记录")

    def _disconnect_all(self) -> None:
        print("  文件已关闭")

    def _cleanup_hook(self) -> None:
        print("  (钩子) 清理临时状态完成")


class DatabaseMigration(DataMigration):
    """数据库迁移 (模拟)"""

    def __init__(self):
        self._records: list[dict] = []
        self._errors: list[str] = []

    def _connect_source(self) -> None:
        print("  连接到源数据库 (localhost:5432)")

    def _extract(self) -> list[dict]:
        print("  执行 SELECT * FROM users ...")
        self._records = [
            {"id": "1", "name": "Alice", "email": "alice@example.com"},
            {"id": "2", "name": "Bob", "email": "bob@example.com"},
            {"id": "3", "name": "Charlie", "email": "charlie@example.com"},
        ]
        print(f"  抽取 {len(self._records)} 条用户记录")
        return self._records

    def _transform(self, data: list[dict]) -> list[dict]:
        """覆写钩子: 数据清洗"""
        print("  (钩子) 执行数据清洗...")
        cleaned = []
        for rec in data:
            # 邮箱小写, 去除空格
            rec["email"] = rec["email"].strip().lower()
            rec["name"] = rec["name"].strip()
            cleaned.append(rec)
        print(f"  清洗完成, {len(cleaned)} 条记录")
        return cleaned

    def _load(self, data: list[dict]) -> None:
        print("  执行 INSERT INTO target_users ...")
        for rec in data:
            print(f"    插入: {rec['name']} <{rec['email']}>")
        print(f"  已加载 {len(data)} 条记录到目标数据库")

    def _disconnect_all(self) -> None:
        print("  关闭数据库连接")

    def _cleanup_hook(self) -> None:
        print("  (钩子) 记录迁移日志到 /var/log/migration.log")
        with open(tempfile.mktemp(suffix=".log"), "w") as f:
            f.write(f"迁移完成, {len(self._records)} 条记录\n")


# =================================================================
# 示例 2: 饮料制作
# =================================================================

class Beverage(abc.ABC):
    """饮料制作模板"""

    def prepare(self) -> None:
        """模板方法"""
        self._boil_water()
        self._brew()
        self._pour_in_cup()
        if self._customer_wants_condiments():  # 钩子方法
            self._add_condiments()
        self._done()

    def _boil_water(self) -> None:
        print("  烧开水 (100°C)")

    def _pour_in_cup(self) -> None:
        print("  倒入杯中")

    @abc.abstractmethod
    def _brew(self) -> None:
        """冲泡"""
        ...

    @abc.abstractmethod
    def _add_condiments(self) -> None:
        """添加调料"""
        ...

    def _customer_wants_condiments(self) -> bool:
        """钩子方法: 默认需要调料"""
        return True

    def _done(self) -> None:
        print("  饮料制作完成! 请享用.")


class Tea(Beverage):
    def _brew(self) -> None:
        print("  用热水浸泡茶叶 3 分钟")

    def _add_condiments(self) -> None:
        print("  加入柠檬片")

    def _customer_wants_condiments(self) -> bool:
        """钩子: 茶一般不加调料, 除非客户要求"""
        return False


class Coffee(Beverage):
    def _brew(self) -> None:
        print("  用热水冲泡咖啡粉, 过滤")

    def _add_condiments(self) -> None:
        print("  加入糖和牛奶")


# =================================================================
# 示例 3: 测试框架模拟
# =================================================================

class TestCase(abc.ABC):
    """junit 风格的测试基类, 展示模板方法在框架中的应用"""

    def run(self) -> None:
        """模板方法: 执行测试"""
        self._setup()
        try:
            self._test()
        except AssertionError as e:
            self._on_failure(str(e))
        except Exception as e:
            self._on_error(str(e))
        else:
            self._on_success()
        finally:
            self._teardown()

    def _setup(self) -> None:
        """前置准备 (钩子)"""
        pass

    @abc.abstractmethod
    def _test(self) -> None:
        """测试方法 (子类实现)"""
        ...

    def _teardown(self) -> None:
        """清理 (钩子)"""
        pass

    def _on_success(self) -> None:
        print(f"  [OK] {self.__class__.__name__}: 通过")

    def _on_failure(self, msg: str) -> None:
        print(f"  [NO] {self.__class__.__name__}: 失败 - {msg}")

    def _on_error(self, msg: str) -> None:
        print(f"  ! {self.__class__.__name__}: 错误 - {msg}")


class MathTest(TestCase):
    def _setup(self) -> None:
        print("  (setup) 初始化测试数据")
        self._a = 10
        self._b = 5

    def _test(self) -> None:
        assert self._a + self._b == 15
        assert self._a - self._b == 5
        assert self._a * self._b == 50
        print("  数学断言全部通过")

    def _teardown(self) -> None:
        print("  (teardown) 清理测试数据")


class FailingTest(TestCase):
    def _test(self) -> None:
        assert 1 + 1 == 3, "故意的失败断言"


# =================================================================
# 主函数
# =================================================================

def main():
    print("=" * 60)
    print("模板方法模式 演示")
    print("=" * 60)

    # ── 创建临时 CSV 文件 ──
    csv_content = "name,age,city\n张三,28,北京\n李四,32,上海\n王五,25,广州\n"
    tmp_dir = tempfile.mkdtemp(prefix="migration_")
    csv_path = os.path.join(tmp_dir, "input.csv")
    json_path = os.path.join(tmp_dir, "output.json")
    with open(csv_path, "w") as f:
        f.write(csv_content)

    # 1) 数据迁移示例
    print("\n--- 1. CSV -> JSON 迁移 ---")
    migration = CSVToJSONMigration(csv_path, json_path)
    migration.migrate()

    print("\n--- 2. 数据库迁移 ---")
    db_migration = DatabaseMigration()
    db_migration.migrate()

    # 2) 饮料制作示例
    print("\n--- 3. 制作茶 ---")
    Tea().prepare()

    print("\n--- 4. 制作咖啡 ---")
    Coffee().prepare()

    # 3) 测试框架示例
    print("\n--- 5. 测试框架 ---")
    MathTest().run()
    FailingTest().run()

    # 清理
    print("\n--- 6. 清理临时文件 ---")
    os.remove(csv_path)
    os.remove(json_path)
    os.rmdir(tmp_dir)

    print("\n" + "=" * 60)
    print("模板方法核心要素")
    print("=" * 60)
    print("  * 模板方法: 定义算法骨架, 子类不能覆写")
    print("  * 抽象方法: 子类必须实现")
    print("  * 具体方法: 所有子类共享的实现")
    print("  * 钩子方法: 子类可选覆写, 控制决策点")
    print("=" * 60)


if __name__ == "__main__":
    main()
