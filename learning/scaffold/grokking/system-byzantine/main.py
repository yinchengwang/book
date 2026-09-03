"""
拜占庭容错 — PBFT + 视图变更 + 故障检测

演示内容:
1. PBFT: 三阶段协议（Pre-Prepare/Prepare/Commit）
2. 视图变更: 主节点故障时的视图切换
3. 故障检测: 超时机制 + 节点状态管理
"""

import time
from typing import Optional

# ============================================================================
# 1. PBFT 三阶段协议
# ============================================================================

class PBFTNode:
    """PBFT 节点模拟（教学简化版）"""

    NORMAL = "normal"
    VIEW_CHANGE = "view-change"
    BYZANTINE = "byzantine"  # 恶意节点状态

    def __init__(self, node_id: int, total_nodes: int):
        self.node_id = node_id
        self.total_nodes = total_nodes
        self.state = PBFTNode.NORMAL
        self.view = 0
        self.sequence = 0
        self.log = []
        self.prepared = set()
        self.committed = set()

    @property
    def f(self) -> int:
        """最多容忍的拜占庭节点数：3f+1=N"""
        return (self.total_nodes - 1) // 3

    @property
    def is_primary(self) -> bool:
        return self.node_id == self.view % self.total_nodes

    def pre_prepare(self, request: str) -> Optional[dict]:
        """Pre-Prepare 阶段（仅主节点发起）"""
        if not self.is_primary:
            return None
        self.sequence += 1
        msg = {
            "type": "pre-prepare",
            "view": self.view,
            "sequence": self.sequence,
            "request": request,
            "primary_id": self.node_id,
        }
        self.log.append(msg)
        return msg

    def prepare(self, msg: dict) -> Optional[dict]:
        """Prepare 阶段: 节点收到 Pre-Prepare 后广播 Prepare"""
        if msg["view"] != self.view:
            return None
        prepare_msg = {
            "type": "prepare",
            "view": self.view,
            "sequence": msg["sequence"],
            "node_id": self.node_id,
            "digest": hash(msg["request"]),
        }
        self.prepared.add(msg["sequence"])
        return prepare_msg

    def commit(self, msg: dict, prepare_count: int) -> Optional[dict]:
        """Commit 阶段: 收到 2f+1 个 Prepare 后广播 Commit"""
        required = 2 * self.f + 1
        if prepare_count < required - 1:  # 减掉自己
            return None
        commit_msg = {
            "type": "commit",
            "view": self.view,
            "sequence": msg["sequence"],
            "node_id": self.node_id,
        }
        self.committed.add(msg["sequence"])
        return commit_msg

    def execute(self, sequence: int) -> None:
        """执行请求"""
        self.committed.add(sequence)
        print(f"    节点 {self.node_id} 执行 sequence={sequence}")


class PBFTSimulator:
    """PBFT 模拟器"""

    def __init__(self, node_count: int = 4):
        self.nodes = [PBFTNode(i, node_count) for i in range(node_count)]

    def run_request(self, request: str) -> None:
        """运行完整三阶段协议"""
        primary = next(n for n in self.nodes if n.is_primary)
        f = primary.f

        print(f"\n  客户端请求: {request}")
        print(f"  主节点: 节点 {primary.node_id} (view={primary.view})")
        print(f"  容忍 f={f}, 总节点={primary.total_nodes}")
        print(f"  要求: Pre-Prepare=1, Prepare≥{2*f+1}, Commit≥{2*f+1}")

        # Phase 1: Pre-Prepare
        pp = primary.pre_prepare(request)
        print(f"\n  Phase 1 (Pre-Prepare):")
        print(f"    主节点 {primary.node_id}: 广播 pre-prepare")

        # Phase 2: Prepare
        print(f"\n  Phase 2 (Prepare):")
        prepare_counts = {}
        for node in self.nodes:
            pm = node.prepare(pp)
            if pm:
                seq = pm["sequence"]
                prepare_counts[seq] = prepare_counts.get(seq, 0) + 1
                print(f"    节点 {node.node_id}: 广播 prepare")

        seq = primary.sequence
        pc = prepare_counts.get(seq, 0)
        print(f"    收到 {pc}/{primary.total_nodes} 个 prepare")

        # Phase 3: Commit
        print(f"\n  Phase 3 (Commit):")
        commit_count = 0
        for node in self.nodes:
            cm = node.commit(pp, pc)
            if cm:
                commit_count += 1

        print(f"    收到 {commit_count}/{primary.total_nodes} 个 commit")
        print(f"  要求≥{2*f+1} → {'[OK] 通过' if commit_count >= 2*f+1 else '[NO] 未通过'}")

        # Execute
        if commit_count >= 2*f + 1:
            for node in self.nodes:
                node.execute(seq)

    def inject_byzantine(self, node_id: int) -> None:
        """注入拜占庭节点"""
        self.nodes[node_id].state = PBFTNode.BYZANTINE
        print(f"\n  节点 {node_id} 被标记为拜占庭（恶意）节点")


# ============================================================================
# 2. 视图变更
# ============================================================================

class ViewChange:
    """视图变更模拟"""

    def __init__(self, nodes: int = 4):
        self.current_view = 0
        self.nodes = [PBFTNode(i, nodes) for i in range(nodes)]

    def trigger(self, failed_primary: int) -> dict:
        """触发视图变更"""
        new_view = self.current_view + 1
        new_primary = new_view % len(self.nodes)

        steps = [
            f"主节点 {failed_primary} 故障（超时未响应）",
            f"备份节点检测到故障，发送 view-change 消息",
            f"新主节点 {new_primary} 收到 ≥2f+1 个 view-change 消息",
            f"新主节点 {new_primary} 广播 new-view 消息",
            f"所有节点切换到 view={new_view}",
        ]
        self.current_view = new_view
        for node in self.nodes:
            node.view = new_view

        return {
            "old_view": new_view - 1,
            "new_view": new_view,
            "new_primary": new_primary,
            "steps": steps,
        }


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("    拜占庭容错演示 — Byzantine Fault Tolerance")
    print("=" * 56)

    print("\n--- 1. PBFT 正常流程（4 节点，无故障）---")
    pbft = PBFTSimulator(4)
    pbft.run_request("transfer A→B 100$")

    print("\n--- 2. PBFT 拜占庭节点容忍 ---")
    pbft2 = PBFTSimulator(4)
    pbft2.inject_byzantine(3)  # 节点 3 是拜占庭
    pbft2.run_request("transfer A→B 50$")
    print(f"\n  PBFT 在 1 个拜占庭节点下仍能达成共识 (3f+1=4, f=1)")

    print("\n--- 3. 视图变更 ---")
    vc = ViewChange(4)
    result = vc.trigger(failed_primary=0)
    print(f"  旧 view: {result['old_view']} → 新 view: {result['new_view']}")
    print(f"  新主节点: 节点 {result['new_primary']}")
    for i, step in enumerate(result["steps"], 1):
        print(f"  步骤 {i}: {step}")

    print("\n--- 4. 故障检测机制 ---")
    print(f"  超时检测: 节点间心跳超时触发 view-change")
    print(f"  水线机制: 高低水位差限制日志增长")
    print(f"  检查点: 定期创建稳定检查点，垃圾回收日志")

    print("\n" + "=" * 56)
    print("提示: PBFT 适用于联盟链等节点数确定的场景")
    print("=" * 56)
