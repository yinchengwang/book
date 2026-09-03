"""
安全设计 — 认证/授权 + HTTPS + 加密存储

演示内容:
1. 认证 vs 授权: JWT 令牌 + RBAC 权限
2. HTTPS: 证书验证 + 加密通信
3. 加密存储: 哈希 + 盐值存储密码

本演示为教学目的，不适用于生产环境。
"""

import hashlib
import json
import time
from typing import Optional

# ============================================================================
# 1. 认证与授权
# ============================================================================

class JWTSimulator:
    """简化版 JWT 模拟（非 RFC 实现，仅教学用）"""

    def __init__(self, secret: str = "change-me-in-production"):
        self.secret = secret
        self.blacklist = set()

    def generate(self, user_id: int, role: str, expire_s: int = 3600) -> str:
        """模拟 JWT 生成"""
        header = {"alg": "HS256", "typ": "JWT"}
        payload = {
            "user_id": user_id,
            "role": role,
            "iat": int(time.time()),
            "exp": int(time.time()) + expire_s,
        }
        # 实际 JWT 用 base64 编码 + 签名，这里简化
        token = f"sim-jwt-{user_id}-{role}-{payload['exp']}"
        return token

    def verify(self, token: str) -> Optional[dict]:
        """验证 JWT"""
        if token in self.blacklist:
            return None
        parts = token.split("-")
        if len(parts) < 4 or not parts[0] == "sim":
            return None
        return {"user_id": int(parts[2]), "role": parts[3], "valid": True}

    def revoke(self, token: str) -> None:
        """撤销令牌"""
        self.blacklist.add(token)


class RBAC:
    """基于角色的访问控制"""

    def __init__(self):
        self.role_permissions = {}

    def define_role(self, role: str, permissions: list) -> None:
        self.role_permissions[role] = set(permissions)

    def check(self, role: str, action: str) -> bool:
        """检查角色是否有权限执行动作"""
        return action in self.role_permissions.get(role, set())

    def show_roles(self) -> None:
        for role, perms in self.role_permissions.items():
            print(f"    {role}: {', '.join(sorted(perms))}")


# ============================================================================
# 2. HTTPS 简化模拟
# ============================================================================

class TLSSimulator:
    """简化版 TLS 握手模拟"""

    CIPHERS = [
        "TLS_AES_256_GCM_SHA384",
        "TLS_CHACHA20_POLY1305_SHA256",
        "TLS_AES_128_GCM_SHA256",
    ]

    @staticmethod
    def handshake(client_hello: str) -> dict:
        """简化 TLS 1.3 握手流程"""
        steps = [
            "ClientHello (支持的密码套件 + 随机数)",
            "ServerHello (选定的密码套件 + 证书)",
            "Server Certificate (X.509 证书链)",
            "Certificate Verify (数字签名验证)",
            "Finished (加密握手确认)",
        ]
        return {
            "protocol": "TLS 1.3",
            "cipher": TLSSimulator.CIPHERS[0],
            "steps": steps,
        }

    @staticmethod
    def encrypt(plaintext: str, session_key: str) -> str:
        """模拟对称加密（实际是 AES-GCM）"""
        return f"encrypted({plaintext})[key={session_key[:8]}...]"


# ============================================================================
# 3. 加密存储
# ============================================================================

class PasswordHasher:
    """密码哈希（教学用简化）"""

    @staticmethod
    def hash_with_salt(password: str, salt: str = None) -> dict:
        """加盐哈希（实际应用应使用 bcrypt/argon2）"""
        salt = salt or hashlib.sha256(
            f"{password}{time.time()}".encode()).hexdigest()[:16]
        hashed = hashlib.pbkdf2_hmac(
            "sha256", password.encode(), salt.encode(), 100000
        ).hex()
        return {"hash": hashed, "salt": salt}

    @staticmethod
    def verify(password: str, salt: str, stored_hash: str) -> bool:
        """验证密码"""
        computed = hashlib.pbkdf2_hmac(
            "sha256", password.encode(), salt.encode(), 100000
        ).hex()
        return computed == stored_hash


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("      安全设计演示 — Security Design")
    print("=" * 56)

    print("\n--- 1. 认证与授权 ---")
    jwt = JWTSimulator(secret="super-secret-key")
    token = jwt.generate(user_id=1001, role="admin")
    print(f"  生成 JWT: {token}")
    decoded = jwt.verify(token)
    print(f"  验证 JWT: {decoded}")

    # RBAC
    rbac = RBAC()
    rbac.define_role("admin", ["read", "write", "delete", "manage_users"])
    rbac.define_role("editor", ["read", "write"])
    rbac.define_role("viewer", ["read"])
    print(f"\n  RBAC 角色定义:")
    rbac.show_roles()
    print(f"\n  admin 可 delete: {rbac.check('admin', 'delete')}")
    print(f"  viewer 可 write: {rbac.check('viewer', 'write')}")

    # 令牌撤销
    jwt.revoke(token)
    print(f"\n  撤销后验证: {jwt.verify(token)}")

    print("\n--- 2. HTTPS/TLS 1.3 ---")
    tls = TLSSimulator()
    handshake = tls.handshake("TLS 1.3 client hello")
    print(f"  协议: {handshake['protocol']}")
    print(f"  密码套件: {handshake['cipher']}")
    for i, step in enumerate(handshake['steps'], 1):
        print(f"  步骤 {i}: {step}")
    encrypted = tls.encrypt("POST /api/login user=admin", "session-key-abc")
    print(f"  加密数据: {encrypted}")

    print("\n--- 3. 加密存储 ---")
    pwd_hasher = PasswordHasher()
    result = pwd_hasher.hash_with_salt("MyP@ssw0rd!")
    print(f"  盐值: {result['salt']}")
    print(f"  Hash: {result['hash'][:20]}...")

    # 验证
    ok = pwd_hasher.verify("MyP@ssw0rd!", result['salt'], result['hash'])
    print(f"  正确密码验证: {'[OK]' if ok else '[NO]'}")
    fail = pwd_hasher.verify("wrong-password", result['salt'], result['hash'])
    print(f"  错误密码验证: {'[OK]' if fail else '[NO]'}")

    print("\n--- 4. 安全最佳实践清单 ---")
    practices = [
        "传输层: TLS 1.3，禁止降级到不安全的协议版本",
        "存储层: 密码使用 bcrypt/argon2 加盐哈希",
        "令牌: 使用 JWT 并设置合理的过期时间 (15 min - 1 h)",
        "权限: 最小权限原则，按角色细分权限",
        "注入防护: 参数化查询防止 SQL 注入",
        "XSS/CSRF: 内容安全策略(CSP) + CSRF Token",
        "速率限制: API 级别限流防止暴力破解",
    ]
    for p in practices:
        print(f"  v {p}")

    print("\n" + "=" * 56)
    print("提示: 安全是系统工程，需从架构层面考虑")
    print("=" * 56)
