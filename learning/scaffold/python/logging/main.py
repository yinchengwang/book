#!/usr/bin/env python3
# =============================================================================
# logging 演示脚本
# 演示 Python logging + 配置 + 多 handler
# =============================================================================

import logging
import logging.config
import json
import sys
from pathlib import Path
from datetime import datetime

# =============================================================================
# 1. 基础 logging 用法
# =============================================================================
def demo_basic_logging():
    """演示基础 logging 配置"""
    print("\n" + "=" * 60)
    print("1. 基础 logging 用法")
    print("=" * 60)

    # 获取 logger（默认 root logger）
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)

    # 清除已有的 handler（避免重复）
    logger.handlers.clear()

    # 创建控制台 handler
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(logging.INFO)
    console_format = logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        datefmt='%H:%M:%S'
    )
    console_handler.setFormatter(console_format)
    logger.addHandler(console_handler)

    # 记录日志
    logger.debug("这是 DEBUG 级别日志（不显示）")
    logger.info("这是 INFO 级别日志")
    logger.warning("这是 WARNING 级别日志")
    logger.error("这是 ERROR 级别日志")
    logger.critical("这是 CRITICAL 级别日志")

    print("✓ 基础 logging 演示完成")

# =============================================================================
# 2. 多 Handler 配置
# =============================================================================
def demo_multi_handler():
    """演示多 handler：控制台 + 文件"""
    print("\n" + "=" * 60)
    print("2. 多 Handler 配置（控制台 + 文件）")
    print("=" * 60)

    # 创建带有多 handler 的 logger
    logger = logging.getLogger('multi_handler')
    logger.setLevel(logging.DEBUG)
    logger.handlers.clear()

    # 控制台 handler - 只显示 INFO 及以上
    console = logging.StreamHandler(sys.stdout)
    console.setLevel(logging.INFO)
    console.setFormatter(logging.Formatter('[CONSOLE] %(levelname)s: %(message)s'))

    # 文件 handler - 记录所有级别
    log_dir = Path('/tmp/python_logging_demo')
    log_dir.mkdir(exist_ok=True)
    log_file = log_dir / 'app.log'
    file_handler = logging.FileHandler(log_file, encoding='utf-8')
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(logging.Formatter(
        '%(asctime)s | %(name)s | %(levelname)-8s | %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    ))

    logger.addHandler(console)
    logger.addHandler(file_handler)

    # 记录不同级别的日志
    logger.debug("调试信息：开始执行")
    logger.info("信息：用户登录")
    logger.warning("警告：密码强度不足")
    logger.error("错误：数据库连接失败")
    logger.critical("严重：系统即将崩溃")

    print(f"✓ 日志已写入文件: {log_file}")

# =============================================================================
# 3. JSON 配置方式
# =============================================================================
def demo_json_config():
    """演示通过字典/JSON 配置 logging"""
    print("\n" + "=" * 60)
    print("3. JSON 配置方式")
    print("=" * 60)

    # 定义 logging 配置字典
    config = {
        "version": 1,
        "disable_existing_loggers": False,
        "formatters": {
            "detailed": {
                "format": "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
            },
            "simple": {
                "format": "%(levelname)s: %(message)s"
            }
        },
        "handlers": {
            "console": {
                "class": "logging.StreamHandler",
                "level": "INFO",
                "formatter": "simple",
                "stream": "ext://sys.stdout"
            }
        },
        "loggers": {
            "json_config": {
                "level": "DEBUG",
                "handlers": ["console"]
            }
        },
        "root": {
            "level": "WARNING"
        }
    }

    # 应用配置
    logging.config.dictConfig(config)

    logger = logging.getLogger('json_config')
    logger.debug("JSON 配置的 DEBUG 日志")
    logger.info("JSON 配置的 INFO 日志")
    logger.warning("JSON 配置的 WARNING 日志")

    print("✓ JSON 配置演示完成")

# =============================================================================
# 4. Logger 层级与继承
# =============================================================================
def demo_logger_hierarchy():
    """演示 logger 层级关系"""
    print("\n" + "=" * 60)
    print("4. Logger 层级与继承")
    print("=" * 60)

    # 清除已有 handler
    root = logging.getLogger()
    root.handlers.clear()

    # 配置 root logger
    root.setLevel(logging.WARNING)
    ch = logging.StreamHandler(sys.stdout)
    ch.setFormatter(logging.Formatter('%(name)s [%(levelname)s] %(message)s'))
    root.addHandler(ch)

    # 创建层级 logger
    app_logger = logging.getLogger('myapp')
    db_logger = logging.getLogger('myapp.database')
    http_logger = logging.getLogger('myapp.http')

    app_logger.info("应用日志（root 不显示 INFO）")
    db_logger.warning("数据库警告")
    http_logger.error("HTTP 请求失败")

    print("✓ Logger 层级演示完成")

# =============================================================================
# 5. 旋转日志文件
# =============================================================================
def demo_rotating_file():
    """演示 RotatingFileHandler 和 TimedRotatingFileHandler"""
    print("\n" + "=" * 60)
    print("5. 旋转日志文件")
    print("=" * 60)

    from logging.handlers import RotatingFileHandler, TimedRotatingFileHandler

    log_dir = Path('/tmp/python_logging_demo')
    log_dir.mkdir(exist_ok=True)

    # 旋转日志 handler（文件大小达到 1KB 时轮转，保留 3 个备份）
    rotating = RotatingFileHandler(
        log_dir / 'rotating.log',
        maxBytes=1024,
        backupCount=3,
        encoding='utf-8'
    )
    rotating.setFormatter(logging.Formatter('%(asctime)s %(message)s'))

    logger = logging.getLogger('rotating')
    logger.setLevel(logging.INFO)
    logger.handlers.clear()
    logger.addHandler(rotating)

    # 写入多条日志（触发旋转）
    for i in range(20):
        logger.info(f"日志条目 #{i:02d} - 填充日志文件以触发轮转")

    print(f"✓ 旋转日志已创建在: {log_dir}")
    print(f"  查看文件: ls -la {log_dir}/rotating.log*")

# =============================================================================
# 主函数
# =============================================================================
def main():
    print("\n" + "╔" + "=" * 58 + "╗")
    print("║" + " " * 10 + "Python logging 日志系统演示" + " " * 17 + "║")
    print("║" + " " * 15 + "配置、Handler、层级管理" + " " * 14 + "║")
    print("╚" + "=" * 58 + "╝")

    demo_basic_logging()
    demo_multi_handler()
    demo_json_config()
    demo_logger_hierarchy()
    demo_rotating_file()

    print("\n" + "=" * 60)
    print("演示完成！关键知识点：")
    print("=" * 60)
    print("• logging.getLogger()           # 获取 logger")
    print("• logger.info/warning/error     # 记录不同级别日志")
    print("• Handler (Stream/File/Rotating)# 多目标输出")
    print("• Formatter                     # 日志格式定制")
    print("• logging.config.dictConfig     # 字典配置方式")
    print("=" * 60)

if __name__ == '__main__':
    main()
