"""
Layout algorithm tests
"""

from parser.flowchart import parse
from layout.layered import layout as apply_layout


def test_linear_layout():
    """Test linear flowchart layout"""
    content = """
flowchart TD
    A --> B --> C --> D
"""
    ir = parse(content)
    ir = apply_layout(ir)

    for node in ir.nodes:
        assert node.x is not None
        assert node.y is not None

    # A should be above D
    nodes_by_y = sorted(ir.nodes, key=lambda n: n.y)
    assert nodes_by_y[0].id == "A"
    assert nodes_by_y[-1].id == "D"


def test_branch_layout():
    """Test branching flowchart"""
    content = """
flowchart TD
    A --> B
    A --> C
    B --> D
    C --> D
"""
    ir = parse(content)
    ir = apply_layout(ir)

    for edge in ir.edges:
        assert len(edge.points) >= 2


if __name__ == "__main__":
    test_linear_layout()
    print("test_linear_layout passed")

    test_branch_layout()
    print("test_branch_layout passed")

    print("\nAll layout tests passed!")
