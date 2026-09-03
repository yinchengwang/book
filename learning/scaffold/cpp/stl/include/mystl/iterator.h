/*
 * iterator.h - 迭代器抽象层
 *
 * 对标 <iterator>，纯手写实现
 * 参考：libstdc++ stl_iterator.h
 */

#ifndef MYSTL_ITERATOR_H
#define MYSTL_ITERATOR_H

#include "type_traits.h"
#include <cstddef>  // ptrdiff_t
#include <iostream> // istream, ostream

MYSTL_NAMESPACE_BEGIN

// ============================================================
// 1. 迭代器标签（Iterator Tags）
// ============================================================

struct input_iterator_tag {};
struct output_iterator_tag {};
struct forward_iterator_tag : public input_iterator_tag {};
struct bidirectional_iterator_tag : public forward_iterator_tag {};
struct random_access_iterator_tag : public bidirectional_iterator_tag {};


// ============================================================
// 2. iterator_traits - 迭代器特性萃取
// ============================================================

template <typename Iterator>
struct iterator_traits {
    using iterator_category = typename Iterator::iterator_category;
    using value_type        = typename Iterator::value_type;
    using difference_type   = typename Iterator::difference_type;
    using pointer           = typename Iterator::pointer;
    using reference         = typename Iterator::reference;
};

// 特化：原生指针 T*
template <typename T>
struct iterator_traits<T*> {
    using iterator_category = random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;
};

// 特化：const T*
template <typename T>
struct iterator_traits<const T*> {
    using iterator_category = random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;
};


// ============================================================
// 3. 反向迭代器适配器
// ============================================================

template <typename Iterator>
class reverse_iterator {
protected:
    Iterator current_;

public:
    using iterator_type     = Iterator;
    using iterator_category = typename iterator_traits<Iterator>::iterator_category;
    using value_type        = typename iterator_traits<Iterator>::value_type;
    using difference_type   = typename iterator_traits<Iterator>::difference_type;
    using pointer           = typename iterator_traits<Iterator>::pointer;
    using reference         = typename iterator_traits<Iterator>::reference;

    reverse_iterator() : current_() {}
    explicit reverse_iterator(iterator_type it) : current_(it) {}

    template <typename Iter>
    reverse_iterator(const reverse_iterator<Iter>& other) : current_(other.base()) {}

    iterator_type base() const { return current_; }

    // 解引用返回前一个位置
    reference operator*() const {
        Iterator tmp = current_;
        return *--tmp;
    }
    pointer operator->() const {
        Iterator tmp = current_;
        return &*--tmp;
    }

    reverse_iterator& operator++() { --current_; return *this; }
    reverse_iterator operator++(int) { reverse_iterator tmp = *this; --current_; return tmp; }
    reverse_iterator& operator--() { ++current_; return *this; }
    reverse_iterator operator--(int) { reverse_iterator tmp = *this; ++current_; return tmp; }

    reverse_iterator operator+(difference_type n) const { return reverse_iterator(current_ - n); }
    reverse_iterator& operator+=(difference_type n) { current_ -= n; return *this; }
    reverse_iterator operator-(difference_type n) const { return reverse_iterator(current_ + n); }
    reverse_iterator& operator-=(difference_type n) { current_ += n; return *this; }

    reference operator[](difference_type n) const { return *(*this + n); }
};

// 反向迭代器比较
template <typename Iterator>
bool operator==(const reverse_iterator<Iterator>& a, const reverse_iterator<Iterator>& b) {
    return a.base() == b.base();
}
template <typename Iterator>
bool operator!=(const reverse_iterator<Iterator>& a, const reverse_iterator<Iterator>& b) {
    return a.base() != b.base();
}
template <typename Iterator>
bool operator<(const reverse_iterator<Iterator>& a, const reverse_iterator<Iterator>& b) {
    return b.base() < a.base();
}
template <typename Iterator>
bool operator>(const reverse_iterator<Iterator>& a, const reverse_iterator<Iterator>& b) {
    return b < a;
}
template <typename Iterator>
bool operator<=(const reverse_iterator<Iterator>& a, const reverse_iterator<Iterator>& b) {
    return !(b < a);
}
template <typename Iterator>
bool operator>=(const reverse_iterator<Iterator>& a, const reverse_iterator<Iterator>& b) {
    return !(a < b);
}


// ============================================================
// 4. 迭代器操作函数
// ============================================================

// advance — 让迭代器前进 n 步
template <typename InputIterator, typename Distance>
void _advance(InputIterator& it, Distance n, input_iterator_tag) {
    while (n > 0) { ++it; --n; }
}
template <typename BidirectionalIterator, typename Distance>
void _advance(BidirectionalIterator& it, Distance n, bidirectional_iterator_tag) {
    if (n >= 0) { while (n > 0) { ++it; --n; } }
    else        { while (n < 0) { --it; ++n; } }
}
template <typename RandomAccessIterator, typename Distance>
void _advance(RandomAccessIterator& it, Distance n, random_access_iterator_tag) {
    it += n;
}

template <typename Iterator, typename Distance>
void advance(Iterator& it, Distance n) {
    _advance(it, n, typename iterator_traits<Iterator>::iterator_category());
}

// distance — 计算两个迭代器之间的距离
template <typename InputIterator>
typename iterator_traits<InputIterator>::difference_type
_distance(InputIterator first, InputIterator last, input_iterator_tag) {
    typename iterator_traits<InputIterator>::difference_type n = 0;
    while (first != last) { ++first; ++n; }
    return n;
}
template <typename RandomAccessIterator>
typename iterator_traits<RandomAccessIterator>::difference_type
_distance(RandomAccessIterator first, RandomAccessIterator last, random_access_iterator_tag) {
    return last - first;
}

template <typename Iterator>
typename iterator_traits<Iterator>::difference_type
distance(Iterator first, Iterator last) {
    return _distance(first, last, typename iterator_traits<Iterator>::iterator_category());
}

// next — 返回前进 n 步后的迭代器
template <typename ForwardIterator>
ForwardIterator next(ForwardIterator it,
                     typename iterator_traits<ForwardIterator>::difference_type n = 1) {
    advance(it, n);
    return it;
}

// prev — 返回后退 n 步后的迭代器
template <typename BidirectionalIterator>
BidirectionalIterator prev(BidirectionalIterator it,
                           typename iterator_traits<BidirectionalIterator>::difference_type n = 1) {
    advance(it, -n);
    return it;
}


// ============================================================
// 5. 插入迭代器适配器
// ============================================================

// back_insert_iterator
template <typename Container>
class back_insert_iterator {
protected:
    Container* container_;
public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;

    explicit back_insert_iterator(Container& c) : container_(&c) {}

    back_insert_iterator& operator=(const typename Container::value_type& value) {
        container_->push_back(value);
        return *this;
    }
    back_insert_iterator& operator=(typename Container::value_type&& value) {
        container_->push_back(mystl::move(value));
        return *this;
    }
    back_insert_iterator& operator*() { return *this; }
    back_insert_iterator& operator++() { return *this; }
    back_insert_iterator operator++(int) { return *this; }
};

template <typename Container>
back_insert_iterator<Container> back_inserter(Container& c) {
    return back_insert_iterator<Container>(c);
}

// front_insert_iterator
template <typename Container>
class front_insert_iterator {
protected:
    Container* container_;
public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;

    explicit front_insert_iterator(Container& c) : container_(&c) {}

    front_insert_iterator& operator=(const typename Container::value_type& value) {
        container_->push_front(value);
        return *this;
    }
    front_insert_iterator& operator=(typename Container::value_type&& value) {
        container_->push_front(mystl::move(value));
        return *this;
    }
    front_insert_iterator& operator*() { return *this; }
    front_insert_iterator& operator++() { return *this; }
    front_insert_iterator operator++(int) { return *this; }
};

template <typename Container>
front_insert_iterator<Container> front_inserter(Container& c) {
    return front_insert_iterator<Container>(c);
}

// insert_iterator
template <typename Container>
class insert_iterator {
protected:
    Container* container_;
    typename Container::iterator iter_;
public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;

    insert_iterator(Container& c, typename Container::iterator it)
        : container_(&c), iter_(it) {}

    insert_iterator& operator=(const typename Container::value_type& value) {
        iter_ = container_->insert(iter_, value);
        ++iter_;
        return *this;
    }
    insert_iterator& operator=(typename Container::value_type&& value) {
        iter_ = container_->insert(iter_, mystl::move(value));
        ++iter_;
        return *this;
    }
    insert_iterator& operator*() { return *this; }
    insert_iterator& operator++() { return *this; }
    insert_iterator operator++(int) { return *this; }
};

template <typename Container>
insert_iterator<Container> inserter(Container& c, typename Container::iterator it) {
    return insert_iterator<Container>(c, it);
}


// ============================================================
// 6. 流迭代器
// ============================================================

// istream_iterator
template <typename T, typename Distance = ptrdiff_t>
class istream_iterator {
public:
    using iterator_category = input_iterator_tag;
    using value_type = T;
    using difference_type = Distance;
    using pointer = const T*;
    using reference = const T&;

private:
    std::istream* stream_;
    mutable T value_;
    bool end_marker_;

    void read() {
        if (stream_) {
            end_marker_ = !(*stream_ >> value_);
        }
    }

public:
    // 末尾哨兵
    istream_iterator() : stream_(nullptr), end_marker_(true) {}
    // 绑定流
    istream_iterator(std::istream& s) : stream_(&s), end_marker_(false) { read(); }

    reference operator*() const { return value_; }
    pointer operator->() const { return &value_; }

    istream_iterator& operator++() { read(); return *this; }
    istream_iterator operator++(int) { istream_iterator tmp = *this; read(); return tmp; }

    bool operator==(const istream_iterator& other) const {
        return end_marker_ == other.end_marker_;
    }
    bool operator!=(const istream_iterator& other) const {
        return !(*this == other);
    }
};

// ostream_iterator
template <typename T>
class ostream_iterator {
public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;

private:
    std::ostream* stream_;
    const char* delim_;

public:
    ostream_iterator(std::ostream& s, const char* delim = nullptr)
        : stream_(&s), delim_(delim) {}

    ostream_iterator& operator=(const T& value) {
        *stream_ << value;
        if (delim_) *stream_ << delim_;
        return *this;
    }
    ostream_iterator& operator*() { return *this; }
    ostream_iterator& operator++() { return *this; }
    ostream_iterator operator++(int) { return *this; }
};

MYSTL_NAMESPACE_END

#endif // MYSTL_ITERATOR_H