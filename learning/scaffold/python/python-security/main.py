#!/usr/bin/env python3
"""
security.py — Python 安全编程演示

演示常见安全问题及防御方法。
"""

import hashlib
import hmac
import secrets
import re
from typing import Optional


# ============================================================================
# 1. 密码存储
# ============================================================================

def hash_password(password: str, salt: Optional[bytes] = None) -> tuple:
    """安全存储密码：加盐哈希"""
    if salt is None:
        salt = secrets.token_bytes(32)

    hashed = hashlib.pbkdf2_hmac('sha256', password.encode(), salt, 100000)
    return hashed.hex(), salt.hex()


def verify_password(password: str, hashed: str, salt: str) -> bool:
    """验证密码"""
    new_hash, _ = hash_password(password, bytes.fromhex(salt))
    return hmac.compare_digest(new_hash, hashed)


def demo_password():
    """密码存储演示"""
    print("\n    --- 密码存储 ---")

    password = "MySecurePassword123!"
    hashed, salt = hash_password(password)

    print(f"    原密码: {password}")
    print(f"    哈希: {hashed[:40]}...")
    print(f"    盐: {salt[:20]}...")

    # 验证
    assert verify_password(password, hashed, salt)
    print("    验证成功 [OK]")

    # 错误密码
    assert not verify_password("wrong", hashed, salt)
    print("    错误密码验证失败 [OK]")


# ============================================================================
# 2. SQL 注入防御
# ============================================================================

def demo_sql_injection():
    """SQL 注入防御"""
    print("\n    --- SQL 注入防御 ---")

    # 危险：不使用参数化查询
    user_input = "'; DROP TABLE users; --"

    # 安全：使用参数化查询
    # cursor.execute("SELECT * FROM users WHERE name = ?", (user_input,))
    safe_query = "SELECT * FROM users WHERE name = ?"

    print(f"    危险输入: {user_input}")
    print(f"    安全查询: {safe_query} (参数化)")


# ============================================================================
# 3. XSS 防御
# ============================================================================

def escape_html(text: str) -> str:
    """HTML 转义"""
    return (text
        .replace('&', '&amp;')
        .replace('<', '&lt;')
        .replace('>', '&gt;')
        .replace('"', '&quot;')
        .replace("'", '&#x27;'))


def demo_xss():
    """XSS 防御"""
    print("\n    --- XSS 防御 ---")

    user_input = '<script>alert("xss")</script>'
    escaped = escape_html(user_input)

    print(f"    原始输入: {user_input}")
    print(f"    转义后: {escaped}")


# ============================================================================
# 4. 输入验证
# ============================================================================

def validate_email(email: str) -> bool:
    """邮箱验证"""
    pattern = r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
    return bool(re.match(pattern, email))


def validate_password_strength(password: str) -> tuple:
    """密码强度检查"""
    errors = []
    if len(password) < 8:
        errors.append("至少8个字符")
    if not re.search(r'[A-Z]', password):
        errors.append("需要大写字母")
    if not re.search(r'[a-z]', password):
        errors.append("需要小写字母")
    if not re.search(r'\d', password):
        errors.append("需要数字")
    return len(errors) == 0, errors


def demo_validation():
    """输入验证"""
    print("\n    --- 输入验证 ---")

    # 邮箱验证
    emails = ["test@example.com", "invalid", "user@domain"]
    for email in emails:
        valid = validate_email(email)
        print(f"    {email}: {'有效' if valid else '无效'}")

    # 密码强度
    pwd = "Weak1"
    valid, errors = validate_password_strength(pwd)
    print(f"    密码 '{pwd}': {'强' if valid else '弱'}")
    if errors:
        print(f"      问题: {', '.join(errors)}")


# ============================================================================
# 5. 安全随机数
# ============================================================================

def demo_secure_random():
    """安全随机数"""
    print("\n    --- 安全随机数 ---")

    # 不安全
    import random
    insecure = random.randint(0, 1000)

    # 安全
    secure = secrets.randbelow(1000)
    token = secrets.token_urlsafe(32)

    print(f"    不安全随机: {insecure}")
    print(f"    安全随机: {secure}")
    print(f"    安全令牌: {token[:20]}...")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 安全编程演示")
    print("=" * 60)

    demo_password()
    demo_sql_injection()
    demo_xss()
    demo_validation()
    demo_secure_random()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
