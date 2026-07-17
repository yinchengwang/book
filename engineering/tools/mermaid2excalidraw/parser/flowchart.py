# -*- coding: utf-8 -*-
"""
Flowchart parser for Mermaid to Excalidraw converter
"""

import re
from typing import Dict, List, Optional

from parser.ir import DiagramIR, DiagramType, Edge, EdgeStyle, Node, NodeShape, Subgraph


class FlowchartParser:
    DIRECTIONS = {"TD", "TB", "BT", "RL", "LR"}
    EDGE_MARKERS = ["-->", "---", "-.->", "==>", "-.-"]

    def __init__(self):
        self.ir = DiagramIR(type=DiagramType.FLOWCHART)
        self._nodes: Dict[str, Node] = {}  # Keyed by node ID (not full line)
        self._subgraphs: Dict[str, Subgraph] = {}
        self._edge_counter = 0
        self._pending_edges: List[str] = []

    def parse(self, content: str) -> DiagramIR:
        lines = content.strip().split("\n")

        # First pass: detect direction
        for line in lines:
            line = line.strip()
            if line.startswith("flowchart"):
                match = re.search(r"flowchart\s+(\w+)", line)
                self.ir.metadata["direction"] = match.group(1) if match else "TD"
                break

        # Second pass: collect node definitions first
        node_defs = {}  # node_id -> (label, shape)
        for line in lines:
            line = line.strip()
            if not line or line.startswith("flowchart") or line.startswith("%%"):
                continue
            if line.startswith("subgraph"):
                self._parse_subgraph_def(lines, line)
            else:
                node_info = self._extract_node_def(line)
                if node_info:
                    node_id, label, shape = node_info
                    if node_id not in node_defs:
                        node_defs[node_id] = (label, shape)

        # Create nodes from definitions
        for node_id, (label, shape) in node_defs.items():
            node = Node(id=node_id, label=label, shape=shape)
            self._nodes[node_id] = node
            self.ir.add_node(node)

        # Third pass: parse edges (nodes now exist)
        for line in lines:
            line = line.strip()
            if self._is_edge_line(line):
                self._parse_edge(line)

        # Add subgraphs
        for sg in self._subgraphs.values():
            self.ir.subgraphs.append(sg)

        return self.ir

    def _is_edge_line(self, line: str) -> bool:
        for marker in self.EDGE_MARKERS:
            if marker in line:
                return True
        return False

    def _extract_node_def(self, line: str) -> Optional[tuple]:
        """Extract (node_id, label, shape) from node definition line"""
        # Skip edge lines
        if self._is_edge_line(line):
            return None

        # Patterns with id: id[label], id(label), id((label)), id{label}
        patterns = [
            (r"^(\w+)\(\((.+)\)\)$", NodeShape.CIRCLE),
            (r"^(\w+)\[(.+)\]$", NodeShape.RECT),
            (r"^(\w+)\((.+)\)$", NodeShape.ROUNDED),
            (r"^(\w+)\{(.+)\}$", NodeShape.DIAMOND),
        ]
        for pattern, shape in patterns:
            match = re.match(pattern, line)
            if match:
                return (match.group(1), match.group(2), shape)

        # Simple patterns without id
        simple_patterns = [
            (r"^\(\((.+)\)\)$", NodeShape.CIRCLE),
            (r"^\[(.+)\]$", NodeShape.RECT),
            (r"^\((.+)\)$", NodeShape.ROUNDED),
            (r"^\{(.+)\}$", NodeShape.DIAMOND),
        ]
        for pattern, shape in simple_patterns:
            match = re.match(pattern, line)
            if match:
                label = match.group(1)
                return (label, label, shape)

        # Plain id (only if not an edge)
        if line and re.match(r"^\w+$", line):
            return (line, line, NodeShape.RECT)

        return None

    def _parse_edge(self, line: str) -> None:
        # Extract label first: |label|
        label = None
        label_match = re.search(r"\|([^|]+)\|", line)
        if label_match:
            label = label_match.group(1)
            # Remove label part for clean parsing (e.g., "|Yes|")
            line = line[:label_match.start()] + line[label_match.end():]

        # Determine edge style
        style = EdgeStyle.SOLID
        if "-.->" in line:
            style = EdgeStyle.DASHED
        elif "==>" in line:
            style = EdgeStyle.THICK

        # Split by arrow markers
        # Pattern handles: ---, -->, -.-, ==>, and their variants
        parts = re.split(r"\s*-+>?\s*|\s*=\+=+>?\s*", line)
        parts = [p.strip() for p in parts if p.strip()]
        if len(parts) >= 2:
            # Extract node IDs (remove brackets/labels if any)
            from_raw = parts[0]
            to_raw = parts[-1]

            # Extract pure node ID from potential label
            from_id = self._extract_node_id(from_raw)
            to_id = self._extract_node_id(to_raw)

            # Ensure nodes exist
            if from_id not in self._nodes:
                self._nodes[from_id] = Node(id=from_id, label=from_id)
                self.ir.add_node(self._nodes[from_id])
            if to_id not in self._nodes:
                self._nodes[to_id] = Node(id=to_id, label=to_id)
                self.ir.add_node(self._nodes[to_id])

            self._edge_counter += 1
            self.ir.add_edge(Edge(
                id=f"e{self._edge_counter}",
                from_node=from_id,
                to_node=to_id,
                label=label,
                style=style
            ))

    def _extract_node_id(self, raw: str) -> str:
        """Extract pure node ID from raw string like 'C[label]', 'B{Decision}', or just 'A'"""
        raw = raw.strip()
        # Match patterns like: A[...], A(...), A{...}, A((...))
        match = re.match(r"^(\w+)(?:\[.*\]|\(.*\)|\{.*\}|\(\(.*\)\))?$", raw)
        if match:
            return match.group(1)
        # Fallback: if no brackets found, return as-is
        return raw if raw else "unknown"

    def _parse_subgraph_def(self, lines: List[str], start_line: str) -> None:
        match = re.match(r"subgraph\s+(\w+)\s*(?:\[(.+?)\])?", start_line)
        if not match:
            return
        sg_id = match.group(1)
        sg_label = match.group(2) or sg_id
        self._subgraphs[sg_id] = Subgraph(id=sg_id, label=sg_label)


def parse(content: str) -> DiagramIR:
    return FlowchartParser().parse(content)
