# -*- coding: utf-8 -*-
"""
Layered layout algorithm for flowchart positioning
"""

from collections import defaultdict, deque
from typing import Dict, List

from parser.ir import DiagramIR, Edge, Node


class LayeredLayout:
    def __init__(
        self,
        node_width: float = 120,
        node_height: float = 60,
        h_spacing: float = 50,
        v_spacing: float = 80,
        margin_left: float = 50,
        margin_top: float = 50,
    ):
        self.node_width = node_width
        self.node_height = node_height
        self.h_spacing = h_spacing
        self.v_spacing = v_spacing
        self.margin_left = margin_left
        self.margin_top = margin_top

    def layout(self, ir: DiagramIR) -> DiagramIR:
        if not ir.nodes:
            return ir

        adj = self._build_adjacency(ir.edges)

        # Compute levels
        levels = self._compute_levels(ir.nodes, adj)

        # Group by level
        layers = self._group_by_level(levels)

        # Compute coordinates
        self._compute_coordinates(ir.nodes, layers)

        # Compute edge points
        self._compute_edge_points(ir.nodes, ir.edges)

        return ir

    def _build_adjacency(self, edges: List[Edge]) -> Dict[str, List[str]]:
        adj = defaultdict(list)
        for edge in edges:
            adj[edge.from_node].append(edge.to_node)
        return dict(adj)

    def _compute_levels(self, nodes: List[Node], adj: Dict[str, List[str]]) -> Dict[str, int]:
        all_nodes = {n.id for n in nodes}
        roots = all_nodes - set(adj.keys())

        if not roots and nodes:
            roots = {nodes[0].id}

        levels: Dict[str, int] = {}
        for root in roots:
            levels[root] = 0
            queue = deque([root])
            while queue:
                node_id = queue.popleft()
                current_level = levels[node_id]
                for neighbor in adj.get(node_id, []):
                    if neighbor not in levels or levels[neighbor] < current_level + 1:
                        levels[neighbor] = current_level + 1
                        queue.append(neighbor)

        for node in nodes:
            if node.id not in levels:
                levels[node.id] = 0

        return levels

    def _group_by_level(self, levels: Dict[str, int]) -> Dict[int, List[str]]:
        layers: Dict[int, List[str]] = defaultdict(list)
        for node_id, level in levels.items():
            layers[level].append(node_id)
        return dict(layers)

    def _compute_coordinates(self, nodes: List[Node], layers: Dict[int, List[str]]) -> None:
        node_map = {n.id: n for n in nodes}
        canvas_width = 800

        for level, node_ids in layers.items():
            layer_width = len(node_ids) * self.node_width + (len(node_ids) - 1) * self.h_spacing
            offset_x = self.margin_left + (canvas_width - layer_width) / 2

            for i, node_id in enumerate(node_ids):
                node = node_map.get(node_id)
                if node:
                    node.x = offset_x + i * (self.node_width + self.h_spacing)
                    node.y = self.margin_top + level * (self.node_height + self.v_spacing)
                    node.width = self.node_width
                    node.height = self.node_height

    def _compute_edge_points(self, nodes: List[Node], edges: List[Edge]) -> None:
        node_map = {n.id: n for n in nodes}
        for edge in edges:
            from_node = node_map.get(edge.from_node)
            to_node = node_map.get(edge.to_node)
            if from_node and to_node:
                start_x = from_node.x + from_node.width / 2
                start_y = from_node.y + from_node.height
                end_x = to_node.x + to_node.width / 2
                end_y = to_node.y
                edge.points = [(start_x, start_y), (end_x, end_y)]


def layout(ir: DiagramIR, **kwargs) -> DiagramIR:
    engine = LayeredLayout(**kwargs)
    return engine.layout(ir)
