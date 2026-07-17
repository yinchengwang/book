#!/usr/bin/env python3
# =============================================================================
# debugging 演示脚本
# 演示 pdb + breakpoint + 日志调试
# =============================================================================

import sys
import logging
import traceback

# =============================================================================
# 1. print 调试（最基础）
# =============================================================================
def demo_print_debug():
    """演示 print 调试法"""
    print("\n" + "=" * 60)
    print("1. print 调试法")
    print("=" * 60)

    def divide(a, b):
        print(f"[DEBUG] divide({a}, {b}) called")
        result = a / b
        print(f"[DEBUG] result = {result}")
        return result

    print("测试正常情况：")
    result = divide(10, 2)
    print(f"  结果: {result}")

    print("测试异常情况：")
    try:
        result = divide(10, 0)
    except ZeroDivisionError as e:
        print(f"[ERROR] {e}")

    print("✓ print 调试演示完成")

# =============================================================================
# 2. logging 调试
# =============================================================================
def demo_logging_debug():
    """演示 logging 调试"""
    print("\n" + "=" * 60)
    print("2. logging 调试")
    print("=" * 60)

    # 配置调试日志
    logging.basicConfig(
        level=logging.DEBUG,
        format='%(asctime)s | %(levelname)-8s | %(name)s | %(message)s',
        datefmt='%H:%M:%S'
    )

    logger = logging.getLogger('debug_demo')

    def process_data(data):
        logger.debug(f"process_data 输入: {data}")
        if not data:
            logger.warning("空数据输入")
            return []
        result = [x * 2 for x in data]
        logger.info(f"处理完成: {len(result)} 个元素")
        return result

    process_data([1, 2, 3, 4, 5])
    process_data([])
    print("✓ logging 调试演示完成")

# =============================================================================
# 3. pdb 模拟演示
# =============================================================================
def demo_pdb_simulation():
    """模拟 pdb 调试（非交互式演示）"""
    print("\n" + "=" * 60)
    print("3. pdb 交互式调试（模拟）")
    print("=" * 60)

    print("""
pdb 常用命令：
  l (list)       列出当前行附近的代码
  n (next)       执行下一行
  s (step)       进入函数
  c (continue)   继续执行
  p (print)      打印变量值
  pp (pretty)    漂亮打印
  q (quit)       退出调试器

在代码中设置断点：
  import pdb; pdb.set_trace()   # 传统方式
  breakpoint()                   # Python 3.7+ 推荐方式
""")

    print("✓ pdb 演示完成")

# =============================================================================
# 4. 异常追踪
# =============================================================================
def demo_traceback():
    """演示异常追踪"""
    print("\n" + "=" * 60)
    print("4. 异常追踪与回溯")
    print("=" * 60)

    def level3():
        raise ValueError("深层错误")

    def level2():
        level3()

    def level1():
        level2()

    try:
        level1()
    except ValueError:
        print("捕获的异常追踪：")
        traceback.print_exc(file=sys.stdout)

    print("✓ 异常追踪演示完成")

# =============================================================================
# 5. 常见调试技巧
# =============================================================================
def demo_debug_tips():
    """演示调试技巧"""
    print("\n" + "=" * 60)
    print("5. 调试技巧总结")
    print("=" * 60)

    print("""
【变量检查】
  • type(var)          # 检查变量类型
  • dir(obj)           # 查看对象属性和方法
  • vars(obj)          # 查看对象 __dict__
  • inspect.getsource  # 查看源码

【断言】
  • assert condition, message  # 运行时检查

【性能分析】
  • import timeit      # 微基准测试
  • import cProfile    # 代码性能分析
  • import tracemalloc # 内存使用追踪

【Python 3.7+ 新特性】
  • breakpoint()       # 内置断点函数
  • PYTHONBREAKPOINT   # 环境变量控制调试器

【设置 PYTHONBREAKPOINT】
  export PYTHONBREAKPOINT=0           # 禁用所有 breakpoint()
  export PYTHONBREAKPOINT=pudb.set_trace  # 使用 pudb
  export PYTHONBREAKPOINT=ipdb.set_trace  # 使用 ipdb
""")

    print("✓ 调试技巧演示完成")

# =============================================================================
# 主函数
# =============================================================================
def main():
    print("\n" + "╔" + "=" * 58 + "╗")
    print("║" + " " * 15 + "Python 调试技巧演示" + " " * 22 + "║")
    print("║" + " " * 10 + "pdb + breakpoint + 日志调试" + " " * 12 + "║")
    print("╚" + "=" * 58 + "╝")

    demo_print_debug()
    demo_logging_debug()
    demo_pdb_simulation()
    demo_traceback()
    demo_debug_tips()

    print("\n" + "=" * 60)
    print("演示完成！关键知识点：")
    print("=" * 60)
    print("• breakpoint()            # Python 3.7+ 内置断点")
    print("• logging.debug()         # 日志调试")
    print("• traceback.print_exc()   # 打印异常堆栈")
    print("• assert condition        # 断言检查")
    print("• cProfile / timeit       # 性能分析")
    print("=" * 60)

if __name__ == '__main__':
    main()
