"""
MiniVecDB 过滤器构造器
支持元数据过滤
"""
from typing import Any, Dict, List, Optional, Union


class Filter:
    """过滤器构造器"""

    def __init__(self):
        self._conditions: List[Dict[str, Any]] = []
        self._logic: str = "$and"  # 默认 AND

    def equal(self, field: str, value: Any) -> "Filter":
        """精确匹配"""
        self._conditions.append({field: value})
        return self

    def not_equal(self, field: str, value: Any) -> "Filter":
        """不等于"""
        self._conditions.append({field: {"$ne": value}})
        return self

    def greater_than(self, field: str, value: Union[int, float]) -> "Filter":
        """大于"""
        self._conditions.append({field: {"$gt": value}})
        return self

    def greater_than_or_equal(self, field: str, value: Union[int, float]) -> "Filter":
        """大于等于"""
        self._conditions.append({field: {"$gte": value}})
        return self

    def less_than(self, field: str, value: Union[int, float]) -> "Filter":
        """小于"""
        self._conditions.append({field: {"$lt": value}})
        return self

    def less_than_or_equal(self, field: str, value: Union[int, float]) -> "Filter":
        """小于等于"""
        self._conditions.append({field: {"$lte": value}})
        return self

    def in_list(self, field: str, values: List[Any]) -> "Filter":
        """在列表中"""
        self._conditions.append({field: {"$in": values}})
        return self

    def not_in_list(self, field: str, values: List[Any]) -> "Filter":
        """不在列表中"""
        self._conditions.append({field: {"$nin": values}})
        return self

    def and_filter(self, *filters: "Filter") -> "Filter":
        """组合为 AND 条件"""
        conditions = []
        for f in filters:
            conditions.extend(f._conditions)
        self._conditions.extend(conditions)
        return self

    def or_filter(self, *filters: "Filter") -> "Filter":
        """组合为 OR 条件"""
        or_conditions = []
        for f in filters:
            if f._conditions:
                or_conditions.append({"$and": f._conditions})
        if or_conditions:
            self._conditions.append({"$or": or_conditions})
        return self

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        if len(self._conditions) == 0:
            return {}
        if len(self._conditions) == 1:
            return self._conditions[0]
        return {self._logic: self._conditions}
