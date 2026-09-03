"""
中间表示 (Intermediate Representation) 模块

定义 Mermaid 图表解析后的统一数据结构
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional, Tuple


class DiagramType(Enum):
    """图表类型枚举"""
    FLOWCHART = "flowchart"
    SEQUENCE = "sequence"
    CLASS = "class"
    STATE = "state"
    UNKNOWN = "unknown"


class NodeShape(Enum):
    """节点形状枚举"""
    RECT = "rect"                    # [label] - 矩形
    ROUNDED = "rounded"              # (label) - 圆角矩形
    STADIUM = "stadium"              # (label) - 同上
    DIAMOND = "diamond"              # {label} - 菱形
    CIRCLE = "circle"                # ((label)) - 圆形
    CYLINDER = "cylinder"            # [(label)] - 圆柱形
    PARALLELOGRAM = "parallelogram"  # /label/ - 平行四边形
    HEXAGON = "hexagon"              # {{label}} - 六边形
    SUBROUTINE = "subroutine"        # [[label]] - 子程序框
    TEXT = "text"                    # 普通文本
    ROUNDED_RECT = "roundRect"       # 圆角矩形（同 stadium）


class EdgeStyle(Enum):
    """边样式枚举"""
    SOLID = "solid"           # --> 实线
    DASHED = "dashed"         # -.-> 虚线
    DOTTED = "dotted"         # -.->
    THICK = "thick"           # ==> 加粗


@dataclass
class Node:
    """图节点"""
    id: str                           # 节点唯一标识
    label: str                        # 显示文本
    shape: NodeShape = NodeShape.RECT # 形状类型
    style: Optional[Dict[str, Any]] = None  # 额外样式
    x: Optional[float] = None         # X 坐标（布局后填充）
    y: Optional[float] = None         # Y 坐标（布局后填充）
    width: Optional[float] = None     # 宽度（布局后填充）
    height: Optional[float] = None    # 高度（布局后填充）

    def __hash__(self):
        return hash(self.id)


@dataclass
class Edge:
    """图边/连线"""
    id: str                                    # 边唯一标识
    from_node: str                             # 起始节点 ID
    to_node: str                               # 目标节点 ID
    label: Optional[str] = None                # 边标签
    style: EdgeStyle = EdgeStyle.SOLID         # 线条样式
    stroke_color: Optional[str] = None         # 自定义颜色
    points: List[Tuple[float, float]] = field(default_factory=list)  # 路径点（布局后填充）

    def __hash__(self):
        return hash(self.id)


@dataclass
class Subgraph:
    """子图容器"""
    id: str
    label: str
    nodes: List[str] = field(default_factory=list)  # 包含的节点 ID 列表
    x: Optional[float] = None
    y: Optional[float] = None
    width: Optional[float] = None
    height: Optional[float] = None


@dataclass
class DiagramIR:
    """图表中间表示

    统一表示所有类型的 Mermaid 图表
    """
    type: DiagramType = DiagramType.UNKNOWN
    nodes: List[Node] = field(default_factory=list)
    edges: List[Edge] = field(default_factory=list)
    subgraphs: List[Subgraph] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)

    def get_node(self, node_id: str) -> Optional[Node]:
        """根据 ID 查找节点"""
        for node in self.nodes:
            if node.id == node_id:
                return node
        return None

    def get_edges_from(self, node_id: str) -> List[Edge]:
        """获取从指定节点出发的所有边"""
        return [e for e in self.edges if e.from_node == node_id]

    def get_edges_to(self, node_id: str) -> List[Edge]:
        """获取指向指定节点的所有边"""
        return [e for e in self.edges if e.to_node == node_id]

    def add_node(self, node: Node) -> None:
        """添加节点"""
        if self.get_node(node.id) is None:
            self.nodes.append(node)

    def add_edge(self, edge: Edge) -> None:
        """添加边"""
        self.edges.append(edge)


@dataclass
class SequenceParticipant:
    """时序图参与者"""
    id: str
    name: str
    x: float = 0


@dataclass
class SequenceMessage:
    """时序图消息"""
    id: str
    from_participant: str
    to_participant: str
    type: str  # sync, async, return, etc.
    label: str
    y: float = 0  # Y 坐标（布局后填充）


@dataclass
class SequenceNote:
    """时序图注释"""
    placement: str  # over, left, right, etc.
    participants: List[str]
    text: str
    y: float = 0


@dataclass
class SequenceIR:
    """时序图中间表示"""
    participants: List[SequenceParticipant] = field(default_factory=list)
    messages: List[SequenceMessage] = field(default_factory=list)
    notes: List[SequenceNote] = field(default_factory=list)
