## ADDED: dict-hmm-oov

### 概述

新增基于隐马尔可夫模型（HMM）的未登录词识别能力。当 Trie 无法匹配连续 CJK 字符片段时，自动使用 Viterbi 算法进行序列标注（B/M/E/S），从标注序列中提取词边界。

### API

```c
int32_t dict_enable_hmm(dict_t *dict, bool enabled);
int32_t dict_set_hmm_model(dict_t *dict, const dict_hmm_model_t *model);
```

- `dict_enable_hmm()`：启用/禁用 HMM OOV 识别，新建 dict 默认为启用
- `dict_set_hmm_model()`：设置自定义 HMM 模型（NULL 则恢复内置默认模型）

### HMM 模型定义

```c
typedef struct dict_hmm_model_t {
    float start_prob[4];          // B, E, M, S 的初始概率
    float trans_prob[4][4];       // 4×4 转移概率矩阵
    // 发射概率为运行时查表：Unicode 码点 → (B, E, M, S) 概率
    // 使用 compact 格式：uint16 数组中二分查找码点
} dict_hmm_model_t;
```

### Viterbi 解码算法

```
输入：rune 数组 (dict_rune_t[]) 长度 n，所有 rune 均为 Trie 未登录
输出：B/M/E/S 标签序列 labels[0..n-1]

1. 初始化：dp[0][state] = start_prob[state] * emit_prob(rune[0], state)
2. 递推：dp[i][s] = max_{s'} dp[i-1][s'] * trans_prob[s'][s] * emit_prob(rune[i], s)
3. 终止：取 max_{s} dp[n-1][s]
4. 回溯：从 dp[n-1] 最大值状态沿 backptr 回退得标签序列
```

所有概率运算使用 `log` 空间避免浮点下溢。

### 词提取规则

从 B/M/E/S 标签序列提取词：
- **B** 开始一个新词，直到遇到 **E** 结束
- **S** 独立成词
- 不允许的状态转移（如 B→B、E→M）在 Viterbi 中通过 `-∞` 转移概率禁止

### 集成点

在 `dict_segment_cjk()` 中：
1. `dict_collect_runes()` 收集 CJK rune 片段
2. `dict_build_route()` 计算 Trie DP 路径
3. 遍历路径，检测连续未登录字符段（Trie 中无任何匹配的 rune 序列）
4. 对每段 OOV rune 序列调用 `dict_hmm_viterbi()`
5. 从标签序列切割 token，标记为 OOV token

Token 结构不变（`token_t`），OOV token 的 `text` 指向堆上分配的字符串副本。

### 约束

- HMM 仅在 Trie 完全无法覆盖的连续 CJK 片段上激活
- 纯 ASCII 文本不触发 HMM（由 `dict_is_ascii_word_char()` 分流）
- 嵌入式默认模型数据为只读静态数组，不占用堆
- `dict_set_hmm_model()` 传入的 model 由调用方管理生命周期
