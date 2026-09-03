"""
CLI 执行器 - 通过 subprocess 调用 db_cli 工具

支持：
- 单条 SQL 执行
- JSON 输出解析
- 错误处理
"""

import json
import subprocess
import os
import tempfile
from typing import Optional, Dict, Any, List, Tuple


class CLIExecutor:
    """
    CLI 执行器

    通过调用 db_cli 工具执行 SQL 语句并解析结果
    """

    def __init__(self, database: str, cli_path: Optional[str] = None, timeout: int = 30):
        """
        初始化 CLI 执行器

        参数:
            database: 数据库文件路径
            cli_path: db_cli 工具路径，默认在 PATH 中查找
            timeout: 命令超时时间（秒）
        """
        self.database = os.path.abspath(database)
        self.timeout = timeout

        # 查找 CLI 工具
        if cli_path:
            self.cli_path = cli_path
        else:
            self.cli_path = self._find_cli()

        # 检查 CLI 是否存在
        if not os.path.exists(self.cli_path):
            raise FileNotFoundError(
                f"db_cli not found at {self.cli_path}. "
                "Please build the CLI tool first."
            )

    def _find_cli(self) -> str:
        """
        查找 db_cli 工具路径

        查找顺序:
        1. 环境变量 BUILD_MY_DB_CLI
        2. 当前目录下的 db_cli
        3. build/ 目录下的 db_cli
        4. PATH 中的 db_cli
        """
        # 1. 环境变量
        env_path = os.environ.get("BUILD_MY_DB_CLI")
        if env_path and os.path.exists(env_path):
            return env_path

        # 2. 当前目录和常见路径
        search_paths = [
            "./db_cli",
            "./build/db_cli",
            "../build/db_cli",
            "./bin/db_cli",
        ]

        for path in search_paths:
            if os.path.exists(path):
                return path

        # 3. PATH 中查找
        import shutil
        if shutil.which("db_cli"):
            return "db_cli"

        # 4. 常见安装路径
        common_paths = [
            "/usr/local/bin/db_cli",
            "/usr/bin/db_cli",
            "C:\\code\\book\\build\\db_cli.exe",
        ]

        for path in common_paths:
            if os.path.exists(path):
                return path

        raise FileNotFoundError(
            "db_cli not found. Please set BUILD_MY_DB_CLI environment variable "
            "or ensure db_cli is in PATH."
        )

    def execute(self, sql: str) -> Tuple[Optional[List], Optional[List]]:
        """
        执行单条 SQL 语句

        参数:
            sql: SQL 语句

        返回:
            (columns, rows) 元组
            - columns: 列名列表，如 ["id", "name"]
            - rows: 行数据列表，如 [[1, "Alice"], [2, "Bob"]]

        异常:
            OperationalError: 执行失败
            ProgrammingError: SQL 语法错误
        """
        # 构建命令
        cmd = [
            self.cli_path,
            "-c", sql,
            "--json",  # JSON 输出模式
            "-d", self.database,
        ]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout,
                encoding='utf-8'
            )
        except subprocess.TimeoutExpired:
            from .exceptions import OperationalError
            raise OperationalError(f"Query timeout after {self.timeout} seconds")

        except Exception as e:
            from .exceptions import InterfaceError
            raise InterfaceError(f"Failed to execute query: {e}")

        # 检查返回码
        if result.returncode != 0:
            from .exceptions import _translate_error
            error = _translate_error(result.returncode, result.stderr)
            raise error

        # 解析 JSON 输出
        try:
            # 尝试从输出中提取 JSON
            output = result.stdout.strip()

            # 处理可能的额外输出（如 "操作成功" 消息）
            if output.startswith("操作成功") or output.startswith("执行时间"):
                # 没有结果集的命令（INSERT/UPDATE/DELETE）
                return None, None

            # 尝试解析 JSON
            data = json.loads(output)
            return data.get("columns"), data.get("rows")

        except json.JSONDecodeError:
            # 如果不是 JSON，可能是没有结果集的查询
            return None, None

    def execute_many(self, sql: str, params_list: List[Tuple]) -> int:
        """
        执行多条 SQL 语句（使用不同参数）

        参数:
            sql: SQL 语句（包含 %s 占位符）
            params_list: 参数列表

        返回:
            影响的行数
        """
        total_rows = 0
        for params in params_list:
            # 格式化 SQL
            formatted_sql = sql % params if params else sql
            try:
                self.execute(formatted_sql)
                total_rows += 1
            except Exception:
                raise

        return total_rows

    def execute_script(self, script: str) -> List[Tuple[Optional[List], Optional[List]]]:
        """
        执行多条 SQL 语句（分号分隔）

        参数:
            script: SQL 脚本（多条语句用分号分隔）

        返回:
            每条语句的结果列表
        """
        results = []
        statements = [s.strip() for s in script.split(';') if s.strip()]

        for stmt in statements:
            try:
                result = self.execute(stmt)
                results.append(result)
            except Exception:
                # 回滚已执行的操作
                try:
                    self.execute("ROLLBACK")
                except:
                    pass
                raise

        return results

    def get_tables(self) -> List[str]:
        """获取所有表名"""
        try:
            self.execute("SHOW TABLES")
            columns, rows = self.execute("SHOW TABLES")
            if rows:
                return [row[0] for row in rows]
            return []
        except:
            return []

    def get_schema(self, table_name: str) -> Dict[str, Any]:
        """获取表结构"""
        try:
            columns, rows = self.execute(f"DESCRIBE {table_name}")
            if rows:
                return {
                    "columns": columns,
                    "rows": rows
                }
            return {}
        except:
            return {}