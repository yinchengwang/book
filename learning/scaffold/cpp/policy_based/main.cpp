/**
 * @file main.cpp
 * @brief Policy 模式演示
 *
 * 演示：Policy 类模板 + 组合策略 + 编译期多态
 * 编译：g++ -std=c++17 -o main main.cpp
 */

#include <iostream>
#include <memory>
#include <vector>
#include <algorithm>
#include <mutex>
#include <sstream>

using std::cout;
using std::endl;

// ============================================================================
// 1. 基本 Policy 定义
// ============================================================================
struct LoggingPolicy {
    static void log(const char* msg) { std::cout << "[LOG] " << msg << endl; }
};

struct NoLoggingPolicy {
    static void log(const char*) {}  // 空实现
};

// ============================================================================
// 2. 线程安全 Policy
// ============================================================================
struct SingleThreaded {
    void lock() {}
    void unlock() {}
};

struct MultiThreaded {
    std::unique_ptr<std::mutex> mtx = std::make_unique<std::mutex>();
    void lock() { mtx->lock(); }
    void unlock() { mtx->unlock(); }
};

// ============================================================================
// 3. 分配策略 Policy
// ============================================================================
struct HeapAllocation {
    template<typename T>
    static T* create() { return new T; }

    template<typename T>
    static void destroy(T* p) { delete p; }
};

struct PoolAllocation {
    template<typename T>
    static T* create() { return static_cast<T*>(::operator new(sizeof(T))); }

    template<typename T>
    static void destroy(T* p) { ::operator delete(p); }
};

// ============================================================================
// 4. SmartPtr 的 Policy 组合
// ============================================================================
template<typename T, typename ThreadModel = SingleThreaded, typename AllocModel = HeapAllocation>
class SmartPtr {
public:
    explicit SmartPtr(T* p = nullptr) : ptr_(p) {}

    ~SmartPtr() {
        if (ptr_) {
            AllocModel::destroy(ptr_);
        }
    }

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }

    // Policy 组合示例
    void safeOperation() {
        ThreadModel tm;
        tm.lock();
        cout << "Thread-safe operation" << endl;
        tm.unlock();
    }

private:
    T* ptr_;
};

// ============================================================================
// 5. Storage 类的 Policy 组合
// ============================================================================
template<typename T>
class StorageInterface {
public:
    virtual ~StorageInterface() = default;
    virtual void push(const T& item) = 0;
    virtual T pop() = 0;
    virtual bool empty() const = 0;
};

// Policy: 序列化方式
struct JsonSerializer {
    static void serialize(std::ostream& os, const std::string& data) {
        os << "\"" << data << "\"";
    }
};

struct XmlSerializer {
    static void serialize(std::ostream& os, const std::string& data) {
        os << "<data>" << data << "</data>";
    }
};

// Policy: 存储后端
struct MemoryStorage {
    static std::vector<std::string>& getStorage() {
        static std::vector<std::string> storage;
        return storage;
    }
};

struct FileStorage {
    static void persist(const std::string& filename) {
        cout << "Persisting to file: " << filename << endl;
    }
};

// 带 Policy 的 Storage 类
template<typename T, typename Serializer = JsonSerializer, typename Backend = MemoryStorage>
class PolicyStorage : public StorageInterface<T> {
public:
    void push(const T& item) override {
        auto& storage = Backend::getStorage();
        std::ostringstream oss;
        Serializer::serialize(oss, item);
        storage.push_back(oss.str());
    }

    T pop() override {
        auto& storage = Backend::getStorage();
        if (storage.empty()) return T();
        auto item = storage.back();
        storage.pop_back();
        return item;
    }

    bool empty() const override {
        return Backend::getStorage().empty();
    }

    void persist(const std::string& filename) {
        Backend::persist(filename);
    }
};

// ============================================================================
// 6. 编译期策略选择
// ============================================================================
template<bool UseCache>
struct CachePolicy {
    static constexpr bool enabled = UseCache;
    static const char* name() { return UseCache ? "Cached" : "NoCache"; }
};

// ============================================================================
// 7. 组合多个 Policy
// ============================================================================
template<typename... Policies>
class CompositePolicy : public Policies... {
public:
    template<typename P>
    bool has() const { return std::is_base_of<P, CompositePolicy>::value; }
};

// ============================================================================
// 客户端代码
// ============================================================================
#include <sstream>
#include <mutex>

int main() {
    cout << "=== Policy 模式演示 ===" << endl << endl;

    // 1. 日志 Policy
    cout << "1. Logging Policy:" << endl;
    LoggingPolicy::log("This is a log message");
    NoLoggingPolicy::log("This won't be printed");

    // 2. 线程安全 Policy
    cout << "\n2. Thread Safety Policy:" << endl;
    SingleThreaded st;
    st.lock();
    cout << "Single-threaded operation" << endl;
    st.unlock();

    // 3. SmartPtr Policy 组合
    cout << "\n3. SmartPtr with Policies:" << endl;
    SmartPtr<int, MultiThreaded, PoolAllocation> sp(new int(42));
    cout << "Value: " << *sp << endl;
    sp.safeOperation();

    // 4. Storage Policy 组合
    cout << "\n4. Storage with Policies:" << endl;
    PolicyStorage<std::string, JsonSerializer, MemoryStorage> jsonStorage;
    jsonStorage.push("hello");
    jsonStorage.push("world");
    cout << "JSON: " << jsonStorage.pop() << endl;

    PolicyStorage<std::string, XmlSerializer, MemoryStorage> xmlStorage;
    xmlStorage.push("hello");
    cout << "XML: " << xmlStorage.pop() << endl;

    // 5. 编译期策略选择
    cout << "\n5. Compile-Time Policy Selection:" << endl;
    cout << "Cache enabled: " << CachePolicy<true>::name() << endl;
    cout << "Cache disabled: " << CachePolicy<false>::name() << endl;

    // 6. Policy 在 STL 中的应用
    cout << "\n6. STL Policy Examples:" << endl;
    std::vector<int> v = {3, 1, 4, 1, 5};
    std::sort(v.begin(), v.end(), std::less<int>());  // LessThan 策略
    cout << "Sorted (less): ";
    for (int x : v) cout << x << " ";
    cout << endl;

    std::sort(v.begin(), v.end(), std::greater<int>());  // GreaterThan 策略
    cout << "Sorted (greater): ";
    for (int x : v) cout << x << " ";
    cout << endl;

    // 7. 分配器 Policy
    cout << "\n7. Allocator Policy:" << endl;
    cout << "Heap allocation: new/delete" << endl;
    cout << "Pool allocation: pre-allocated buffer" << endl;

    return 0;
}
