"""
End-to-end integration tests
"""

from parser.flowchart import parse
from parser.ir import DiagramType
from layout.layered import layout as apply_layout
from generator.excalidraw import generate as generate_excalidraw


def test_full_pipeline():
    """Test complete conversion pipeline"""
    content = """
flowchart TD
    A[Start] --> B{Decision}
    B -->|Yes| C[Process]
    B -->|No| D[End]
    C --> D
"""
    ir = parse(content)
    assert len(ir.nodes) == 4
    assert len(ir.edges) == 4

    ir = apply_layout(ir)
    result = generate_excalidraw(ir)

    assert result["type"] == "excalidraw"
    assert result["version"] == 2
    assert "elements" in result


def test_layout_applied():
    """Test that layout assigns coordinates"""
    content = """
flowchart TD
    A --> B --> C --> D
"""
    ir = parse(content)
    ir = apply_layout(ir)

    for node in ir.nodes:
        assert node.x is not None
        assert node.y is not None


def test_edge_points():
    """Test edge points after layout"""
    content = """
flowchart TD
    A --> B
"""
    ir = parse(content)
    ir = apply_layout(ir)

    for edge in ir.edges:
        assert len(edge.points) >= 2


if __name__ == "__main__":
    test_full_pipeline()
    print("test_full_pipeline passed")

    test_layout_applied()
    print("test_layout_applied passed")

    test_edge_points()
    print("test_edge_points passed")

    print("\nAll integration tests passed!")
