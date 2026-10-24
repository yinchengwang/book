# -*- coding: utf-8 -*-
"""
Default style configuration for Excalidraw output
"""

from typing import Dict
from parser.ir import NodeShape

HAND_DRAWN_STYLE = {
    "strokeColor": "#1a1a1a",
    "strokeWidth": 2,
    "strokeStyle": "solid",
    "fillStyle": "solid",
    "roughness": 1,
    "fontFamily": 1,
    "fontSize": 14,
}

LIGHT_THEME = {
    "background": "#ffffff",
    "text": "#1a1a1a",
    "node_rect": "#e7f5ff",
    "node_rounded": "#e7f5ff",
    "node_diamond": "#fff9db",
    "node_circle": "#2f9e44",
    "node_subroutine": "#fff9db",
}

DARK_THEME = {
    "background": "#1a1a2e",
    "text": "#f7fafc",
    "node_rect": "#2d3748",
    "node_rounded": "#2d3748",
    "node_diamond": "#744210",
    "node_circle": "#22543d",
    "node_subroutine": "#744210",
}


def get_shape_fill_color(shape: NodeShape, theme: str = "light") -> str:
    colors = LIGHT_THEME if theme == "light" else DARK_THEME
    mapping = {
        NodeShape.RECT: "node_rect",
        NodeShape.ROUNDED: "node_rounded",
        NodeShape.STADIUM: "node_rounded",
        NodeShape.DIAMOND: "node_diamond",
        NodeShape.CIRCLE: "node_circle",
        NodeShape.SUBROUTINE: "node_subroutine",
    }
    return colors.get(mapping.get(shape, "node_rect"), colors["node_rect"])


def get_theme_colors(theme: str = "light") -> Dict[str, str]:
    return LIGHT_THEME if theme == "light" else DARK_THEME


def get_node_style(shape: NodeShape, theme: str = "light") -> Dict:
    colors = get_theme_colors(theme)
    fill_color = get_shape_fill_color(shape, theme)
    return {
        **HAND_DRAWN_STYLE,
        "fillColor": fill_color,
        "textColor": colors["text"],
    }


def get_edge_style(theme: str = "light") -> Dict:
    colors = get_theme_colors(theme)
    return {
        **HAND_DRAWN_STYLE,
        "strokeColor": colors.get("text", "#1a1a1a"),
    }
