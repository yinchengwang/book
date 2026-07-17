# -*- coding: utf-8 -*-
"""
Excalidraw JSON generator
"""

from typing import Any, Dict, List
from parser.ir import DiagramIR, Edge, Node, NodeShape
from style.default import get_edge_style, get_node_style, get_theme_colors


class ExcalidrawGenerator:
    def __init__(self, theme: str = "light"):
        self.theme = theme
        self.elements: List[Dict[str, Any]] = []
        self._id_counter = 0

    def generate(self, ir: DiagramIR) -> Dict[str, Any]:
        self.elements = []

        for node in ir.nodes:
            self._generate_node(node)

        for edge in ir.edges:
            self._generate_edge(edge)
            if edge.label:
                self._generate_edge_label(edge)

        colors = get_theme_colors(self.theme)

        return {
            "type": "excalidraw",
            "version": 2,
            "source": "mermaid2excalidraw",
            "elements": self.elements,
            "appState": {
                "gridSize": None,
                "viewBackgroundColor": colors["background"]
            }
        }

    def _next_id(self) -> str:
        self._id_counter += 1
        return f"m2e_{self._id_counter}"

    def _generate_node(self, node: Node) -> None:
        if node.x is None or node.y is None:
            return

        style = get_node_style(node.shape, self.theme)
        w = node.width or 120
        h = node.height or 60

        element = {
            "id": self._next_id(),
            "type": "rectangle",
            "x": node.x,
            "y": node.y,
            "width": w,
            "height": h,
            "strokeColor": style["strokeColor"],
            "backgroundColor": style["fillColor"],
            "strokeWidth": style["strokeWidth"],
        }
        self.elements.append(element)

        # Add label
        text_elem = {
            "id": self._next_id(),
            "type": "text",
            "x": node.x + w / 2,
            "y": node.y + h / 2,
            "width": w - 10,
            "height": h - 10,
            "text": node.label,
            "fontSize": style["fontSize"],
            "fontFamily": style["fontFamily"],
            "textAlign": "center",
            "verticalAlign": "middle",
        }
        self.elements.append(text_elem)

    def _generate_edge(self, edge: Edge) -> None:
        if not edge.points or len(edge.points) < 2:
            return

        style = get_edge_style(self.theme)
        start = edge.points[0]
        end = edge.points[-1]

        element = {
            "id": self._next_id(),
            "type": "arrow",
            "x": start[0],
            "y": start[1],
            "points": [[0, 0], [end[0] - start[0], end[1] - start[1]]],
            "strokeColor": style["strokeColor"],
            "strokeWidth": style["strokeWidth"],
            "endArrowhead": "arrow",
        }
        self.elements.append(element)

    def _generate_edge_label(self, edge: Edge) -> None:
        if not edge.points or not edge.label:
            return

        mid = len(edge.points) // 2
        p1 = edge.points[max(0, mid - 1)]
        p2 = edge.points[min(mid, len(edge.points) - 1)]
        mid_x = (p1[0] + p2[0]) / 2
        mid_y = (p1[1] + p2[1]) / 2

        text_elem = {
            "id": self._next_id(),
            "type": "text",
            "x": mid_x,
            "y": mid_y - 20,
            "width": len(edge.label) * 8,
            "height": 20,
            "text": edge.label,
            "fontSize": 12,
            "fontFamily": 1,
            "textAlign": "center",
            "verticalAlign": "middle",
        }
        self.elements.append(text_elem)


def generate(ir: DiagramIR, theme: str = "light") -> Dict[str, Any]:
    return ExcalidrawGenerator(theme).generate(ir)
