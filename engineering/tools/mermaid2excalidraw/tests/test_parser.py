"""
解析器单元测试
"""

from parser.flowchart import parse
from parser.ir import DiagramType, NodeShape


def test_simple_flowchart():
    """测试简单流程图"""
    content = """
flowchart TD
    A --> B
    B --> C
"""
    ir = parse(content)
    assert ir.type == DiagramType.FLOWCHART
    assert len(ir.nodes) == 3
    assert len(ir.edges) == 2


def test_node_shapes():
    """测试不同节点形状"""
    content = """
flowchart TD
    A[矩形]
    B(圆角)
    C((圆形))
    D{菱形}
"""
    ir = parse(content)
    assert len(ir.nodes) == 4
    shape_map = {n.id: n.shape for n in ir.nodes}
    assert shape_map["A"] == NodeShape.RECT
    assert shape_map["B"] == NodeShape.ROUNDED
    assert shape_map["C"] == NodeShape.CIRCLE
    assert shape_map["D"] == NodeShape.DIAMOND


def test_edge_with_label():
    """测试带标签的边"""
    content = """
flowchart TD
    A -->|yes| B
    A -->|no| C
"""
    ir = parse(content)
    assert len(ir.edges) == 2
    labels = {e.label for e in ir.edges}
    assert "yes" in labels
    assert "no" in labels


if __name__ == "__main__":
    test_simple_flowchart()
    print("test_simple_flowchart passed")
    test_node_shapes()
    print("test_node_shapes passed")
    test_edge_with_label()
    print("test_edge_with_label passed")
    print("\nAll parser tests passed!")
