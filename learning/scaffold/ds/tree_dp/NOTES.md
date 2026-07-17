# 工程对照说明

## 与 engineering/src/db/ 的关联

### 决策树构建

在数据库查询优化器中，决策树用于代价估算和计划选择：

```c
// optimizer_cost.c 中的代价模型
typedef struct {
    double row_count;      /* 估计行数 */
    double cpu_cost;       /* CPU 代价 */
    double io_cost;        /* I/O 代价 */
    PathType type;         /* 路径类型：SeqScan/IndexScan/NestedLoop */
} PathCost;

typedef struct {
    int rel_id;            /* 关系 ID */
    PathCost cheapest;     /* 最小代价路径 */
    List *pathlist;        /* 可选路径列表 */
} RelOptInfo;

/* 决策：选择最小代价的访问路径 */
PathCost choose_best_path(RelOptInfo *rel)
{
    PathCost best = {DBL_MAX, 0, 0};
    for (ListCell *lc = rel->pathlist; lc; lc = lc->next) {
        Path *path = (Path *)lc->ptr_value;
        if (path->total_cost < best.total_cost) {
            best = path->cost;
        }
    }
    return best;
}
```

这与树形 DP 的"选或不选"决策模式一致。

### XML/JSON 解析树

数据库存储 XML/JSON 数据时，解析器构建语法树：

```c
// json_parser.c 中的 AST 节点
typedef enum {
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_NUMBER,
    JSON_BOOL,
    JSON_NULL
} JsonType;

typedef struct JsonNode {
    JsonType type;
    char *key;              /* 对象键 */
    union {
        char *str_val;
        double num_val;
        bool bool_val;
        struct JsonNode **children;  /* 数组或对象元素 */
        int child_count;
    } val;
    struct JsonNode *next;  /* 同级下一个节点 */
} JsonNode;

/* 树形 DP：计算 JSON 树的最大深度 */
int json_max_depth(JsonNode *node)
{
    if (!node) return 0;
    if (node->type != JSON_OBJECT && node->type != JSON_ARRAY) return 1;

    int max_child = 0;
    for (int i = 0; i < node->val.child_count; i++) {
        int child_depth = json_max_depth(node->val.children[i]);
        if (child_depth > max_child) max_child = child_depth;
    }
    return max_child + 1;
}
```

### 查询计划树

数据库查询优化器输出执行计划树：

```c
// planner.c 中的执行计划节点
typedef enum {
    PLAN_SEQ_SCAN,
    PLAN_INDEX_SCAN,
    PLAN_NESTED_LOOP,
    PLAN_HASH_JOIN,
    PLAN_SORT,
    PLAN_AGG
} PlanType;

typedef struct Plan {
    PlanType type;
    int rows;                   /* 估计行数 */
    double total_cost;          /* 总代价 */
    List *targetlist;           /* 输出列 */
    struct Plan *left;          /* 左子计划 */
    struct Plan *right;         /* 右子计划 */
    void *extra;                /* 类型特定数据 */
} Plan;

/* 自底向上计算计划代价 */
double plan_cost(Plan *p)
{
    if (!p) return 0;
    double left_cost = plan_cost(p->left);
    double right_cost = plan_cost(p->right);

    switch (p->type) {
        case PLAN_NESTED_LOOP:
            return left_cost + p->rows * right_cost;
        case PLAN_HASH_JOIN:
            return left_cost + right_cost + hash_build_cost(p);
        // ...
    }
}
```

### 组织结构与权限树

数据库权限系统使用树形结构：

```c
// auth.c 中的角色层次
typedef struct Role {
    Oid oid;
    char *rolename;
    Oid parent_oid;          /* 父角色 */
    List *member_roles;      /* 直接成员 */
    Bitmapset *permissions;  /* 权限位图 */
} Role;

/* 检查用户是否有某权限（沿树上溯） */
bool role_has_permission(Role *role, Permission perm)
{
    while (role) {
        if (bms_is_member(perm, role->permissions)) {
            return true;
        }
        role = get_role_by_oid(role->parent_oid);
    }
    return false;
}
```

### 目录层次与命名空间

数据库使用目录树管理对象命名空间：

```c
// catalog.c 中的命名空间
typedef struct Namespace {
    Oid oid;
    char *nspname;           /* 命名空间名 */
    Oid parent_nsp;          /* 父命名空间 */
    List *tables;            /* 直接包含的表 */
    List *child_nsps;        /* 子命名空间 */
} Namespace;

/* 查找完整路径下的对象 */
ObjectAddress resolve_qualified_name(const char *schema, const char *table)
{
    Namespace *ns = get_namespace(schema);
    if (!ns) ereport(ERROR, "schema not found");

    /* 在命名空间中查找表 */
    ObjectAddress addr = search_table_in_namespace(ns, table);
    if (!OidIsValid(addr.objectId)) {
        /* 在父命名空间中继续查找 */
        if (ns->parent_nsp != InvalidOid) {
            return resolve_qualified_name(get_namespace_name(ns->parent_nsp), table);
        }
        ereport(ERROR, "table not found");
    }
    return addr;
}
```

### 性能优化：尾递归优化

编译器会将简单的树形 DP 尾递归转换为迭代：

```c
// 尾递归版本（可能被优化）
int dfs_depth(Node *u, int parent, int depth)
{
    int max_depth = depth;
    for (Node *c = u->children; c; c = c->next) {
        if (c != parent) {
            int child_depth = dfs_depth(c, u, depth + 1);
            if (child_depth > max_depth) max_depth = child_depth;
        }
    }
    return max_depth;
}
```

GCC 使用 `-O2` 时会自动将尾递归转换为循环：
```makefile
CFLAGS = -Wall -Wextra -O2 -foptimize-sibling-calls
```
