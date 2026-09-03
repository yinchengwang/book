"""
消息队列设计 — MQ 模式 + 顺序消息 + 幂等消费

演示内容:
1. 消息队列模式: 点对点/发布订阅
2. 顺序消息: 分区内严格有序
3. 幂等消费: 去重机制实现
"""

import time
import hashlib
from collections import defaultdict

# ============================================================================
# 1. 消息队列核心
# ============================================================================

class Message:
    """消息"""

    def __init__(self, topic: str, key: str, body: str,
                 msg_id: str = None):
        self.topic = topic
        self.key = key
        self.body = body
        self.msg_id = msg_id or hashlib.md5(
            f"{topic}:{key}:{time.time()}".encode()).hexdigest()[:8]
        self.timestamp = time.time()


class MessageQueue:
    """消息队列（点对点模式 + 发布订阅）"""

    def __init__(self):
        self.queues = defaultdict(list)       # queue_name → [messages]
        self.topics = defaultdict(list)       # topic → [subscribers]
        self.subscriber_queues = defaultdict(dict)  # subscriber → {topic → [messages]}

    def send_to_queue(self, queue: str, msg: Message) -> None:
        """点对点: 发送到队列"""
        self.queues[queue].append(msg)

    def consume_queue(self, queue: str):
        """点对点: 消费队列（每条消息仅一个消费者获取）"""
        if self.queues[queue]:
            return self.queues[queue].pop(0)
        return None

    def subscribe(self, topic: str, subscriber: str) -> None:
        """发布订阅: 订阅主题"""
        if subscriber not in self.topics[topic]:
            self.topics[topic].append(subscriber)
        if subscriber not in self.subscriber_queues:
            self.subscriber_queues[subscriber] = defaultdict(list)

    def publish(self, topic: str, msg: Message) -> None:
        """发布订阅: 发布消息到主题"""
        for sub in self.topics[topic]:
            self.subscriber_queues[sub][topic].append(msg)

    def poll(self, subscriber: str, topic: str):
        """发布订阅: 拉取消息"""
        q = self.subscriber_queues.get(subscriber, {}).get(topic, [])
        if q:
            return q.pop(0)
        return None


# ============================================================================
# 2. 顺序消息
# ============================================================================

class OrderedMessageQueue:
    """分区内有序消息队列"""

    def __init__(self, partition_count: int = 3):
        self.partitions = {i: [] for i in range(partition_count)}

    def _get_partition(self, key: str) -> int:
        """按 key 哈希决定分区"""
        return int(hashlib.md5(key.encode()).hexdigest(), 16) % len(self.partitions)

    def send(self, key: str, body: str) -> dict:
        """同 key 的消息进入同一分区，保证顺序"""
        pid = self._get_partition(key)
        msg = Message("ordered", key, body)
        self.partitions[pid].append(msg)
        return {"partition": pid, "offset": len(self.partitions[pid]) - 1}

    def consume(self, partition: int):
        """按分区消费"""
        q = self.partitions.get(partition, [])
        return q.pop(0) if q else None

    def status(self) -> dict:
        return {f"partition-{p}": len(q)
                for p, q in self.partitions.items()}


# ============================================================================
# 3. 幂等消费
# ============================================================================

class IdempotentConsumer:
    """幂等消费者——已处理消息去重"""

    def __init__(self):
        self.processed_ids = set()
        self.duplicates = 0
        self.processed = 0

    def process(self, msg: Message) -> str:
        """消费消息（幂等保证）"""
        if msg.msg_id in self.processed_ids:
            self.duplicates += 1
            return "duplicate-skipped"

        # 模拟业务处理
        self.processed_ids.add(msg.msg_id)
        self.processed += 1
        return f"processed: {msg.body[:30]}..."

    def stats(self) -> dict:
        return {
            "processed": self.processed,
            "duplicates_skipped": self.duplicates,
            "dedup_rate": f"{self.duplicates / (self.processed + self.duplicates):.0%}"
            if (self.processed + self.duplicates) > 0 else "0%",
        }


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("    消息队列设计演示 — Message Queue")
    print("=" * 56)

    print("\n--- 1. 点对点模式 ---")
    mq = MessageQueue()
    mq.send_to_queue("order_paid", Message("order", "1", "订单 #1001 已支付"))
    mq.send_to_queue("order_paid", Message("order", "2", "订单 #1002 已支付"))
    mq.send_to_queue("order_paid", Message("order", "3", "订单 #1003 已支付"))

    for i in range(3):
        msg = mq.consume_queue("order_paid")
        print(f"  消费者 {i+1} 获取: [{msg.msg_id}] {msg.body}")
    more = mq.consume_queue("order_paid")
    print(f"  队列空后消费: {more}")

    print("\n--- 2. 发布订阅模式 ---")
    mq.subscribe("order_created", "notification-service")
    mq.subscribe("order_created", "analytics-service")
    mq.subscribe("order_created", "search-indexer")
    mq.publish("order_created", Message("order", "1001", "新订单 #1001"))

    for sub in ["notification-service", "analytics-service", "search-indexer"]:
        msg = mq.poll(sub, "order_created")
        print(f"  {sub} 收到: [{msg.msg_id}] {msg.body}")

    print("\n--- 3. 顺序消息 ---")
    omq = OrderedMessageQueue(partition_count=3)
    orders = [
        ("user:42", "创建订单"), ("user:42", "支付订单"),
        ("user:42", "发货订单"), ("user:99", "创建订单"),
        ("user:99", "取消订单"), ("user:42", "完成订单"),
    ]
    for key, body in orders:
        info = omq.send(key, body)
        print(f"  [{body:<6}] key={key:<10} → {info}")

    print(f"\n  分区状态: {omq.status()}")

    print("\n--- 4. 幂等消费 ---")
    consumer = IdempotentConsumer()
    # 模拟消息重复投递
    msgs = [
        Message("order", "1", "支付回调", "id-001"),
        Message("order", "1", "支付回调", "id-001"),  # 重复
        Message("order", "2", "支付回调", "id-002"),
        Message("order", "1", "支付回调", "id-001"),  # 重复
        Message("order", "3", "支付回调", "id-003"),
    ]
    for msg in msgs:
        result = consumer.process(msg)
        print(f"  消息 {msg.msg_id}: {result}")

    print(f"\n  {consumer.stats()}")

    print("\n" + "=" * 56)
    print("提示: 消息中间件是异步解耦的核心基础设施")
    print("=" * 56)
