# 条件变量 NOTES

## 工程代码交叉引用

### merge_scheduler.c 中的条件变量模式

文件位置：`engineering/src/db/index/vector_index/streaming/merge_scheduler.c`

这是工程代码中条件变量的典型应用场景——任务队列的生产者-消费者模式。

#### 1. 初始化（第82-83行）

```c
pthread_mutex_init(&queue->mutex, NULL);
pthread_cond_init(&queue->cond, NULL);
```

#### 2. 生产者入队（第138行）

```c
pthread_cond_signal(&queue->cond);  // 唤醒一个消费者
```

#### 3. 消费者等待（第152-154行）

```c
while (queue->head == NULL && !pthread_cond_wait(&queue->cond, &queue->mutex) == 0) {
    /* 如果队列为空，等待信号 */
}
```

**关键点**：这里用了 `while` 循环来避免虚假唤醒，但条件判断有些冗余。
正确的标准模式应该是：

```c
while (queue->head == NULL) {
    pthread_cond_wait(&queue->cond, &queue->mutex);
}
```

#### 4. 广播通知（第396行）

```c
pthread_cond_signal(&sched->task_queue.cond);  /* 唤醒线程以便退出 */
```

#### 5. 等待完成（第459行）

```c
while (task_queue_size(&sched->task_queue) > 0) {
    pthread_cond_wait(&sched->done_cond, &sched->state_mutex);
}
```

#### 6. 销毁（第109行）

```c
pthread_cond_destroy(&queue->cond);
```

### 学习要点

| 要点 | 说明 |
|------|------|
| 必须配合 mutex | 条件变量必须与互斥锁一起使用 |
| while 而非 if | 防止虚假唤醒 |
| 锁的粒度 | 只在检查/修改条件时持有锁，wait 中锁自动释放 |
| signal vs broadcast | 只有一个消费者时用 signal，多个时用 broadcast |
| 超时等待 | 可用 `pthread_cond_timedwait` 实现超时逻辑 |

### 相关文件

- `engineering/src/db/index/vector_index/streaming/merge_scheduler.c` - 任务队列实现
- `engineering/include/db/index/vector_index/streaming/merge_scheduler.h` - 头文件
