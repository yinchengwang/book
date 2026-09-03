#!/usr/bin/env python3
# exceptions scaffold — Python 异常处理
#
# 复现命令：
#   python3 main.py
#
# 演示 5 段：
#   [try_except]  — try/except 基本用法
#   [multi]       — 多异常捕获
#   [else_finally]— else 与 finally
#   [raise]       — raise 抛出异常
#   [chain]       — 异常链与自定义异常


def main():
    # === [try_except] try/except 基本用法 ===
    print("[try_except] === try/except 基本用法 ===")

    # 基本异常捕获
    try:
        result = 10 / 0
    except ZeroDivisionError:
        print("  捕获 ZeroDivisionError: 除数不能为零")

    # 捕获异常对象
    try:
        num = int("not_a_number")
    except ValueError as e:
        print(f"  捕获 ValueError: {e}")

    # 通用异常捕获（不推荐）
    try:
        lst = [1, 2, 3]
        value = lst[10]  # IndexError
    except Exception as e:
        print(f"  捕获 Exception: {type(e).__name__} - {e}")

    # === [multi] 多异常捕获 ===
    print("\n[multi] === 多异常捕获 ===")

    def divide_safe(a, b):
        """安全除法"""
        try:
            result = a / b
            return result
        except TypeError:
            print("    类型错误：参数必须是数字")
            return None
        except ZeroDivisionError:
            print("    除数不能为零")
            return None

    print(f"  divide_safe(10, 2)   -> {divide_safe(10, 2)}")
    print(f"  divide_safe(10, 0)   -> {divide_safe(10, 0)}")
    print(f"  divide_safe('a', 2)  -> {divide_safe('a', 2)}")

    # Python 3.10+ 的 match-case 风格
    def parse_int(value):
        try:
            return int(value)
        except (ValueError, TypeError) as e:
            print(f"    解析失败: {e}")
            return None

    print(f"  parse_int('123')     -> {parse_int('123')}")
    print(f"  parse_int('abc')     -> {parse_int('abc')}")

    # === [else_finally] else 与 finally ===
    print("\n[else_finally] === else 与 finally ===")

    def read_file_content(filename):
        """读取文件内容"""
        try:
            f = open(filename, 'r')
        except FileNotFoundError:
            print(f"  文件不存在: {filename}")
            return None
        else:
            # 只有没有异常时执行
            content = f.read()
            f.close()
            return content
        finally:
            # 无论是否有异常都执行
            print(f"  finally: 清理资源 (filename={filename})")

    # 文件不存在，触发 except
    read_file_content("nonexistent.txt")

    # === [raise] raise 抛出异常 ===
    print("\n[raise] === raise 抛出异常 ===")

    def validate_age(age: int) -> int:
        """验证年龄"""
        if age < 0:
            raise ValueError("年龄不能为负数")
        if age > 150:
            raise ValueError("年龄超出合理范围")
        return age

    try:
        validate_age(-5)
    except ValueError as e:
        print(f"  validate_age(-5) 抛出: {e}")

    try:
        validate_age(200)
    except ValueError as e:
        print(f"  validate_age(200) 抛出: {e}")

    print(f"  validate_age(25)  -> {validate_age(25)}")

    # === [chain] 异常链与自定义异常 ===
    print("\n[chain] === 异常链与自定义异常 ===")

    # 自定义异常类
    class ValidationError(Exception):
        """验证错误异常"""
        def __init__(self, field: str, message: str):
            self.field = field
            self.message = message
            super().__init__(f"{field}: {message}")

    def validate_user(username: str, email: str):
        """验证用户信息"""
        errors = []

        if len(username) < 3:
            errors.append("用户名至少3个字符")

        if '@' not in email:
            errors.append("邮箱格式无效")

        if errors:
            raise ValidationError("user", "; ".join(errors))

        return {"username": username, "email": email}

    try:
        validate_user("ab", "invalid")
    except ValidationError as e:
        print(f"  自定义异常: {e}")
        print(f"    field: {e.field}, message: {e.message}")

    # 异常链：保留原始异常
    def inner():
        raise RuntimeError("原始错误")

    def outer():
        try:
            inner()
        except RuntimeError as e:
            # 使用 raise ... from 保留异常链
            raise ValueError("包装后的错误") from e

    try:
        outer()
    except ValueError as e:
        print(f"  异常链: {e}")
        print(f"    原始异常: {e.__cause__}")

    print("\n=== PASS ===")

if __name__ == "__main__":
    main()
