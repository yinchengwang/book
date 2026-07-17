/*
 * rb_tree.h — 红黑树实现
 *
 * 对标 libstdc++ 的 _Rb_tree，纯手写实现
 * 参考：libstdc++ stl_tree.h
 */

#ifndef MYSTL_RB_TREE_H
#define MYSTL_RB_TREE_H

#include "type_traits.h"
#include "allocator.h"
#include "iterator.h"
#include "utility.h"
#include "functional.h"
#include <cstddef>

MYSTL_NAMESPACE_BEGIN

// ============================================================
// 1. 红黑树节点基类
// ============================================================

// 颜色：false = 红色，true = 黑色
using _rb_tree_color = bool;
constexpr bool _rb_tree_red = false;
constexpr bool _rb_tree_black = true;

// 节点基类（不含数据）
struct _rb_tree_node_base {
    _rb_tree_color color;
    _rb_tree_node_base* parent;
    _rb_tree_node_base* left;
    _rb_tree_node_base* right;

    _rb_tree_node_base() : color(_rb_tree_red), parent(nullptr), left(nullptr), right(nullptr) {}
};

// ============================================================
// 2. 红黑树节点（含数据）
// ============================================================

template <typename T>
struct _rb_tree_node : public _rb_tree_node_base {
    T data;

    _rb_tree_node() : _rb_tree_node_base(), data() {}
    _rb_tree_node(const T& val) : _rb_tree_node_base(), data(val) {}
    _rb_tree_node(T&& val) : _rb_tree_node_base(), data(mystl::move(val)) {}

    // 获取节点指针
    static _rb_tree_node<T>* from_base(_rb_tree_node_base* base) {
        return static_cast<_rb_tree_node<T>*>(base);
    }
    static const _rb_tree_node<T>* from_base(const _rb_tree_node_base* base) {
        return static_cast<const _rb_tree_node<T>*>(base);
    }
};

// ============================================================
// 3. 红黑树迭代器
// ============================================================

// 前向声明
_rb_tree_node_base* _rb_tree_increment(_rb_tree_node_base* node);
const _rb_tree_node_base* _rb_tree_increment(const _rb_tree_node_base* node);
_rb_tree_node_base* _rb_tree_decrement(_rb_tree_node_base* node);
const _rb_tree_node_base* _rb_tree_decrement(const _rb_tree_node_base* node);

template <typename T, typename Ref, typename Ptr>
class _rb_tree_iterator {
public:
    using iterator_category = bidirectional_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = Ptr;
    using reference         = Ref;

    using node_base         = _rb_tree_node_base;
    using node_type         = _rb_tree_node<T>;
    using iterator          = _rb_tree_iterator<T, T&, T*>;
    using const_iterator    = _rb_tree_iterator<T, const T&, const T*>;
    using self              = _rb_tree_iterator;

    node_base* node_;

    _rb_tree_iterator() : node_(nullptr) {}
    explicit _rb_tree_iterator(node_base* n) : node_(n) {}
    _rb_tree_iterator(const iterator& it) : node_(it.node_) {}

    reference operator*() const {
        return static_cast<node_type*>(node_)->data;
    }
    pointer operator->() const {
        return &(static_cast<node_type*>(node_)->data);
    }

    self& operator++() {
        node_ = _rb_tree_increment(node_);
        return *this;
    }
    self operator++(int) {
        self tmp = *this;
        node_ = _rb_tree_increment(node_);
        return tmp;
    }
    self& operator--() {
        node_ = _rb_tree_decrement(node_);
        return *this;
    }
    self operator--(int) {
        self tmp = *this;
        node_ = _rb_tree_decrement(node_);
        return tmp;
    }

    bool operator==(const self& other) const { return node_ == other.node_; }
    bool operator!=(const self& other) const { return node_ != other.node_; }
};

template <typename T, typename Ref, typename Ptr>
class _rb_tree_const_iterator {
public:
    using iterator_category = bidirectional_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = Ptr;
    using reference         = Ref;

    using node_base         = _rb_tree_node_base;
    using node_type         = _rb_tree_node<T>;
    using iterator          = _rb_tree_iterator<T, T&, T*>;
    using const_iterator    = _rb_tree_const_iterator;
    using self              = _rb_tree_const_iterator;

    const node_base* node_;

    _rb_tree_const_iterator() : node_(nullptr) {}
    explicit _rb_tree_const_iterator(const node_base* n) : node_(n) {}
    _rb_tree_const_iterator(const iterator& it) : node_(it.node_) {}
    _rb_tree_const_iterator(const const_iterator& it) : node_(it.node_) {}

    self& operator=(const self& other) {
        node_ = other.node_;
        return *this;
    }

    reference operator*() const {
        return static_cast<const node_type*>(node_)->data;
    }
    pointer operator->() const {
        return &(static_cast<const node_type*>(node_)->data);
    }

    self& operator++() {
        node_ = _rb_tree_increment(node_);
        return *this;
    }
    self operator++(int) {
        self tmp = *this;
        node_ = _rb_tree_increment(node_);
        return tmp;
    }
    self& operator--() {
        node_ = _rb_tree_decrement(node_);
        return *this;
    }
    self operator--(int) {
        self tmp = *this;
        node_ = _rb_tree_decrement(node_);
        return tmp;
    }

    bool operator==(const self& other) const { return node_ == other.node_; }
    bool operator!=(const self& other) const { return node_ != other.node_; }
};

// ============================================================
// 4. 红黑树迭代器递增/递减
// ============================================================

// 递增（中序后继）
inline _rb_tree_node_base* _rb_tree_increment(_rb_tree_node_base* node) {
    if (node->right != nullptr) {
        node = node->right;
        while (node->left != nullptr)
            node = node->left;
    } else {
        _rb_tree_node_base* parent = node->parent;
        while (node == parent->right) {
            node = parent;
            parent = parent->parent;
        }
        if (node->right != parent)
            node = parent;
    }
    return node;
}

inline const _rb_tree_node_base* _rb_tree_increment(const _rb_tree_node_base* node) {
    return _rb_tree_increment(const_cast<_rb_tree_node_base*>(node));
}

// 递减（中序前驱）
inline _rb_tree_node_base* _rb_tree_decrement(_rb_tree_node_base* node) {
    if (node->color == _rb_tree_red && node->parent->parent == node) {
        // header 的特殊情况
        node = node->right;
    } else if (node->left != nullptr) {
        node = node->left;
        while (node->right != nullptr)
            node = node->right;
    } else {
        _rb_tree_node_base* parent = node->parent;
        while (node == parent->left) {
            node = parent;
            parent = parent->parent;
        }
        node = parent;
    }
    return node;
}

inline const _rb_tree_node_base* _rb_tree_decrement(const _rb_tree_node_base* node) {
    return _rb_tree_decrement(const_cast<_rb_tree_node_base*>(node));
}

// ============================================================
// 5. 红黑树旋转操作
// ============================================================

// 左旋
inline void _rb_tree_rotate_left(_rb_tree_node_base* x, _rb_tree_node_base*& root) {
    _rb_tree_node_base* y = x->right;
    x->right = y->left;
    if (y->left != nullptr)
        y->left->parent = x;
    y->parent = x->parent;

    if (x == root)
        root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;

    y->left = x;
    x->parent = y;
}

// 右旋
inline void _rb_tree_rotate_right(_rb_tree_node_base* x, _rb_tree_node_base*& root) {
    _rb_tree_node_base* y = x->left;
    x->left = y->right;
    if (y->right != nullptr)
        y->right->parent = x;
    y->parent = x->parent;

    if (x == root)
        root = y;
    else if (x == x->parent->right)
        x->parent->right = y;
    else
        x->parent->left = y;

    y->right = x;
    x->parent = y;
}

// ============================================================
// 6. 红黑树插入再平衡
// ============================================================

inline void _rb_tree_insert_rebalance(_rb_tree_node_base* x, _rb_tree_node_base*& root) {
    x->color = _rb_tree_red;
    while (x != root && x->parent->color == _rb_tree_red) {
        if (x->parent == x->parent->parent->left) {
            _rb_tree_node_base* y = x->parent->parent->right;
            if (y != nullptr && y->color == _rb_tree_red) {
                // Case 1: 叔叔是红色
                x->parent->color = _rb_tree_black;
                y->color = _rb_tree_black;
                x->parent->parent->color = _rb_tree_red;
                x = x->parent->parent;
            } else {
                if (x == x->parent->right) {
                    // Case 2: 叔叔是黑色，x 是右孩子
                    x = x->parent;
                    _rb_tree_rotate_left(x, root);
                }
                // Case 3: 叔叔是黑色，x 是左孩子
                x->parent->color = _rb_tree_black;
                x->parent->parent->color = _rb_tree_red;
                _rb_tree_rotate_right(x->parent->parent, root);
            }
        } else {
            _rb_tree_node_base* y = x->parent->parent->left;
            if (y != nullptr && y->color == _rb_tree_red) {
                x->parent->color = _rb_tree_black;
                y->color = _rb_tree_black;
                x->parent->parent->color = _rb_tree_red;
                x = x->parent->parent;
            } else {
                if (x == x->parent->left) {
                    x = x->parent;
                    _rb_tree_rotate_right(x, root);
                }
                x->parent->color = _rb_tree_black;
                x->parent->parent->color = _rb_tree_red;
                _rb_tree_rotate_left(x->parent->parent, root);
            }
        }
    }
    root->color = _rb_tree_black;
}

// ============================================================
// 7. 红黑树删除再平衡
// ============================================================

inline void _rb_tree_erase_rebalance(_rb_tree_node_base* x, _rb_tree_node_base* parent,
                                      _rb_tree_node_base*& root) {
    while (x != root && (x == nullptr || x->color == _rb_tree_black)) {
        if (x == parent->left) {
            _rb_tree_node_base* w = parent->right;
            if (w->color == _rb_tree_red) {
                // Case 1: 兄弟是红色
                w->color = _rb_tree_black;
                parent->color = _rb_tree_red;
                _rb_tree_rotate_left(parent, root);
                w = parent->right;
            }
            if ((w->left == nullptr || w->left->color == _rb_tree_black) &&
                (w->right == nullptr || w->right->color == _rb_tree_black)) {
                // Case 2: 兄弟的两个孩子都是黑色
                w->color = _rb_tree_red;
                x = parent;
                parent = parent->parent;
            } else {
                if (w->right == nullptr || w->right->color == _rb_tree_black) {
                    // Case 3: 兄弟的左孩子是红色
                    if (w->left != nullptr)
                        w->left->color = _rb_tree_black;
                    w->color = _rb_tree_red;
                    _rb_tree_rotate_right(w, root);
                    w = parent->right;
                }
                // Case 4: 兄弟的右孩子是红色
                w->color = parent->color;
                parent->color = _rb_tree_black;
                if (w->right != nullptr)
                    w->right->color = _rb_tree_black;
                _rb_tree_rotate_left(parent, root);
                break;
            }
        } else {
            _rb_tree_node_base* w = parent->left;
            if (w->color == _rb_tree_red) {
                w->color = _rb_tree_black;
                parent->color = _rb_tree_red;
                _rb_tree_rotate_right(parent, root);
                w = parent->left;
            }
            if ((w->right == nullptr || w->right->color == _rb_tree_black) &&
                (w->left == nullptr || w->left->color == _rb_tree_black)) {
                w->color = _rb_tree_red;
                x = parent;
                parent = parent->parent;
            } else {
                if (w->left == nullptr || w->left->color == _rb_tree_black) {
                    if (w->right != nullptr)
                        w->right->color = _rb_tree_black;
                    w->color = _rb_tree_red;
                    _rb_tree_rotate_left(w, root);
                    w = parent->left;
                }
                w->color = parent->color;
                parent->color = _rb_tree_black;
                if (w->left != nullptr)
                    w->left->color = _rb_tree_black;
                _rb_tree_rotate_right(parent, root);
                break;
            }
        }
    }
    if (x != nullptr)
        x->color = _rb_tree_black;
}

// ============================================================
// 8. 子树最小/最大节点
// ============================================================

inline _rb_tree_node_base* _rb_tree_minimum(_rb_tree_node_base* x) {
    while (x->left != nullptr)
        x = x->left;
    return x;
}

inline _rb_tree_node_base* _rb_tree_maximum(_rb_tree_node_base* x) {
    while (x->right != nullptr)
        x = x->right;
    return x;
}

// ============================================================
// 9. 红黑树主体
// ============================================================

template <typename Key, typename Value, typename KeyOfValue,
          typename Compare, typename Alloc = allocator<Value>>
class rb_tree {
public:
    using key_type        = Key;
    using value_type      = Value;
    using pointer         = value_type*;
    using const_pointer   = const value_type*;
    using reference       = value_type&;
    using const_reference = const value_type&;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using key_compare     = Compare;   // 添加 key_compare 别名

    using iterator       = _rb_tree_iterator<value_type, reference, pointer>;
    using const_iterator = _rb_tree_const_iterator<value_type, const_reference, const_pointer>;

private:
    using node_base     = _rb_tree_node_base;
    using node_type     = _rb_tree_node<value_type>;
    using node_allocator = typename allocator_traits<Alloc>::template rebind<node_type>::other;

    node_allocator alloc_;
    node_base header_;      // 哨兵节点
    size_type size_;        // 节点数量
    Compare comp_;          // 比较器

    // 获取根节点
    node_base*& root() { return header_.parent; }
    node_base* root() const { return header_.parent; }

    // 获取最左/最右节点
    node_base*& leftmost() { return header_.left; }
    node_base* leftmost() const { return header_.left; }
    node_base*& rightmost() { return header_.right; }
    node_base* rightmost() const { return header_.right; }

    // 分配/释放节点
    node_type* allocate_node() {
        return alloc_.allocate(1);
    }
    void deallocate_node(node_type* p) {
        alloc_.deallocate(p, 1);
    }

    // 创建节点
    node_type* create_node(const value_type& val) {
        node_type* p = allocate_node();
        try {
            // 先构造 first（key），再到 second
            alloc_.construct(p, val);
        } catch (...) {
            deallocate_node(p);
            throw;
        }
        return p;
    }

    // 创建节点（移动）
    node_type* create_node(value_type&& val) {
        node_type* p = allocate_node();
        try {
            alloc_.construct(p, mystl::move(val));
        } catch (...) {
            deallocate_node(p);
            throw;
        }
        return p;
    }

    // 销毁节点
    void destroy_node(node_type* p) {
        alloc_.destroy(p);
        deallocate_node(p);
    }

    // 初始化空树
    void initialize() {
        header_.color = _rb_tree_red;
        header_.parent = nullptr;
        header_.left = &header_;
        header_.right = &header_;
        size_ = 0;
    }

    // 清除子树
    void clear_subtree(node_base* x) {
        if (x != nullptr) {
            clear_subtree(x->left);
            clear_subtree(x->right);
            destroy_node(static_cast<node_type*>(x));
        }
    }

    // 查找插入位置（允许重复）
    // BST 遍历: comp_(k, data) 走左，到目标子节点为空时停止
    // 返回: (y=目标节点, x=目标子节点=null, go_left=是否从左到达)
    struct _insert_pos_result {
        node_base* parent;   // 目标节点的父节点
        bool go_left;        // 是否应该插到父节点的左侧
    };
    _insert_pos_result find_insert_pos(const key_type& k) {
        node_base* x = root();
        node_base* y = &header_;
        bool go_left = true;
        while (x != nullptr) {
            y = x;
            go_left = comp_(k, KeyOfValue()(static_cast<node_type*>(x)->data));
            if (go_left) {
                if (x->left == nullptr) break;
                x = x->left;
            } else {
                if (x->right == nullptr) break;
                x = x->right;
            }
        }
        return {y, go_left};
    }

    // 查找插入位置（带重复检查）
    // 遍历方向: comp_(k, data) 走左，到目标子节点为空时停止
    // 返回: first=目标节点, second={go_left方向, 是否可插入}
    pair<node_base*, pair<bool, bool>> find_insert_unique_pos(const key_type& k) {
        node_base* x = root();
        node_base* y = &header_;
        bool go_left = true;
        while (x != nullptr) {
            y = x;
            go_left = comp_(k, KeyOfValue()(static_cast<node_type*>(x)->data));
            if (go_left) {
                if (x->left == nullptr) break;
                x = x->left;
            } else {
                if (x->right == nullptr) break;
                x = x->right;
            }
        }
        // y 是目标插入位置节点，检查重复
        iterator j = iterator(y);
        if (go_left) {
            if (j == begin()) {
                return {y, {true, true}};
            }
            --j;
        }
        if (comp_(KeyOfValue()(*j), k)) {
            return {y, {go_left, true}};
        }
        return {y, {go_left, false}};  // second.second=false: 有重复
    }

public:
    // 构造函数
    rb_tree() : alloc_(), size_(0), comp_() {
        initialize();
    }
    explicit rb_tree(const Compare& comp) : alloc_(), size_(0), comp_(comp) {
        initialize();
    }
    rb_tree(const rb_tree& other) : alloc_(other.alloc_), size_(0), comp_(other.comp_) {
        initialize();
        for (const_iterator it = other.begin(); it != other.end(); ++it)
            insert_equal(*it);
    }
    rb_tree(rb_tree&& other) noexcept : alloc_(mystl::move(other.alloc_)), size_(other.size_), comp_(mystl::move(other.comp_)) {
        header_ = other.header_;
        header_.parent->parent = &header_;
        other.initialize();
    }

    ~rb_tree() {
        clear();
    }

    rb_tree& operator=(const rb_tree& other) {
        if (this != &other) {
            clear();
            for (const_iterator it = other.begin(); it != other.end(); ++it)
                insert_equal(*it);
        }
        return *this;
    }

    rb_tree& operator=(rb_tree&& other) noexcept {
        if (this != &other) {
            clear();
            header_ = other.header_;
            size_ = other.size_;
            comp_ = mystl::move(other.comp_);
            if (root() != nullptr)
                root()->parent = &header_;
            other.initialize();
        }
        return *this;
    }

    // 迭代器
    iterator begin() { return iterator(leftmost()); }
    iterator end() { return iterator(&header_); }
    const_iterator begin() const { return const_iterator(leftmost()); }
    const_iterator end() const { return const_iterator(&header_); }

    // 容量
    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }
    size_type max_size() const noexcept {
        return static_cast<size_type>(-1) / sizeof(node_type);
    }

    // 比较器访问
    key_compare key_comp() const { return comp_; }

    // 插入（允许重复）
    iterator insert_equal(const value_type& val) {
        node_type* new_node = create_node(val);
        auto pos = find_insert_pos(KeyOfValue()(val));
        node_base* y = pos.parent;
        bool go_left = pos.go_left;
        // go_left=true 表示 val < data，插到父节点左侧
        if (y == &header_) {
            root() = new_node;
            leftmost() = new_node;
            rightmost() = new_node;
        } else if (go_left) {
            y->left = new_node;
            if (y == leftmost()) leftmost() = new_node;
        } else {
            y->right = new_node;
            if (y == rightmost()) rightmost() = new_node;
        }
        new_node->parent = y;
        new_node->left = nullptr;
        new_node->right = nullptr;
        _rb_tree_insert_rebalance(new_node, header_.parent);
        ++size_;
        return iterator(new_node);
    }

    // 插入（不允许重复）
    pair<iterator, bool> insert_unique(const value_type& val) {
        auto pos = find_insert_unique_pos(KeyOfValue()(val));
        if (!pos.second.second) {  // second.second = 可否插入
            return pair<iterator, bool>(iterator(pos.first), false);
        }
        node_type* new_node = create_node(val);
        node_base* y = pos.first;
        bool go_left = pos.second.first;  // second.first = 方向
        if (y == &header_) {
            root() = new_node;
            leftmost() = new_node;
            rightmost() = new_node;
        } else if (go_left) {
            y->left = new_node;
            if (y == leftmost()) leftmost() = new_node;
        } else {
            y->right = new_node;
            if (y == rightmost()) rightmost() = new_node;
        }
        new_node->parent = y;
        new_node->left = nullptr;
        new_node->right = nullptr;
        _rb_tree_insert_rebalance(new_node, header_.parent);
        ++size_;
        return pair<iterator, bool>(iterator(new_node), true);
    }

    // 范围插入（不允许重复）
    template <typename InputIterator>
    void insert_unique(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            insert_unique(*first);
        }
    }

    // 范围插入（允许重复）
    template <typename InputIterator>
    void insert_equal(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            insert_equal(*first);
        }
    }

    // 删除节点
    void erase(iterator it) {
        node_base* y = it.node_;
        node_base* x = nullptr;
        node_base* x_parent = nullptr;

        if (y->left == nullptr) {
            x = y->right;
        } else if (y->right == nullptr) {
            x = y->left;
        } else {
            y = _rb_tree_minimum(y->right);
            x = y->right;
        }

        if (y != it.node_) {
            // y 是后继节点：用 y 替换 it.node_
            it.node_->left->parent = y;
            y->left = it.node_->left;
            if (y != it.node_->right) {
                x_parent = y->parent;
                if (x != nullptr)
                    x->parent = y->parent;
                y->parent->left = x;
                y->right = it.node_->right;
                it.node_->right->parent = y;
            } else {
                x_parent = y;
            }
            if (root() == it.node_) {
                root() = y;
            } else if (it.node_->parent->left == it.node_) {
                it.node_->parent->left = y;
            } else {
                it.node_->parent->right = y;
            }
            y->parent = it.node_->parent;
            // 如果被替换的节点是 leftmost，y 成为新的 leftmost
            if (leftmost() == it.node_) {
                leftmost() = _rb_tree_minimum(y);
            }
            mystl::swap(y->color, it.node_->color);
            y = it.node_;
        } else {
            x_parent = y->parent;
            if (x != nullptr)
                x->parent = y->parent;
            if (root() == y) {
                root() = x;
            } else if (y->parent->left == y) {
                y->parent->left = x;
            } else {
                y->parent->right = x;
            }
            if (leftmost() == y) {
                leftmost() = (x == nullptr) ? y->parent : _rb_tree_minimum(x);
            }
            if (rightmost() == y) {
                rightmost() = (x == nullptr) ? y->parent : _rb_tree_maximum(x);
            }
        }

        if (y->color == _rb_tree_black) {
            _rb_tree_erase_rebalance(x, x_parent, header_.parent);
        }
        destroy_node(static_cast<node_type*>(y));
        --size_;
    }

    // const_iterator 版本（用于 set/multiset，其中 iterator == const_iterator）
    iterator erase(const_iterator it) {
        iterator mutable_it(const_cast<node_base*>(it.node_));
        erase(mutable_it);
        return mutable_it;
    }

    // const_iterator 范围删除
    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) {
            const_iterator cur = first;
            ++first;
            erase(cur);
        }
        return iterator(const_cast<node_base*>(last.node_));
    }

    // 查找
    iterator find(const key_type& k) {
        node_base* x = root();
        node_base* y = &header_;
        while (x != nullptr) {
            if (!comp_(KeyOfValue()(static_cast<node_type*>(x)->data), k)) {
                y = x;
                x = x->left;
            } else {
                x = x->right;
            }
        }
        iterator it = iterator(y);
        return (it == end() || comp_(k, KeyOfValue()(*it))) ? end() : it;
    }

    const_iterator find(const key_type& k) const {
        node_base* x = root();
        node_base* y = const_cast<node_base*>(&header_);
        while (x != nullptr) {
            if (!comp_(KeyOfValue()(static_cast<node_type*>(x)->data), k)) {
                y = x;
                x = x->left;
            } else {
                x = x->right;
            }
        }
        const_iterator it = const_iterator(y);
        return (it == end() || comp_(k, KeyOfValue()(*it))) ? end() : it;
    }

    // 计数
    size_type count(const key_type& k) const {
        pair<const_iterator, const_iterator> r = equal_range(k);
        return mystl::distance(r.first, r.second);
    }

    // 下界
    iterator lower_bound(const key_type& k) {
        node_base* x = root();
        node_base* y = &header_;
        while (x != nullptr) {
            if (!comp_(KeyOfValue()(static_cast<node_type*>(x)->data), k)) {
                y = x;
                x = x->left;
            } else {
                x = x->right;
            }
        }
        return iterator(y);
    }

    const_iterator lower_bound(const key_type& k) const {
        node_base* x = root();
        node_base* y = const_cast<node_base*>(&header_);
        while (x != nullptr) {
            if (!comp_(KeyOfValue()(static_cast<node_type*>(x)->data), k)) {
                y = x;
                x = x->left;
            } else {
                x = x->right;
            }
        }
        return const_iterator(y);
    }

    // 上界
    iterator upper_bound(const key_type& k) {
        node_base* x = root();
        node_base* y = &header_;
        while (x != nullptr) {
            if (comp_(k, KeyOfValue()(static_cast<node_type*>(x)->data))) {
                y = x;
                x = x->left;
            } else {
                x = x->right;
            }
        }
        return iterator(y);
    }

    const_iterator upper_bound(const key_type& k) const {
        node_base* x = root();
        node_base* y = const_cast<node_base*>(&header_);
        while (x != nullptr) {
            if (comp_(k, KeyOfValue()(static_cast<node_type*>(x)->data))) {
                y = x;
                x = x->left;
            } else {
                x = x->right;
            }
        }
        return const_iterator(y);
    }

    // 等值范围
    pair<iterator, iterator> equal_range(const key_type& k) {
        return pair<iterator, iterator>(lower_bound(k), upper_bound(k));
    }

    pair<const_iterator, const_iterator> equal_range(const key_type& k) const {
        return pair<const_iterator, const_iterator>(lower_bound(k), upper_bound(k));
    }

    // 清空
    void clear() {
        if (size_ != 0) {
            clear_subtree(root());
            initialize();
            size_ = 0;
        }
    }

    // 交换
    void swap(rb_tree& other) noexcept {
        mystl::swap(header_, other.header_);
        mystl::swap(size_, other.size_);
        mystl::swap(comp_, other.comp_);
        if (root() != nullptr)
            root()->parent = &header_;
        if (other.root() != nullptr)
            other.root()->parent = &other.header_;
    }
};

MYSTL_NAMESPACE_END

#endif // MYSTL_RB_TREE_H
