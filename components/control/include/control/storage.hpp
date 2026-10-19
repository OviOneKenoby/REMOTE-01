#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string_view>
#include <utility>

namespace control {
// One serialized owner. This allocator is deliberately independent of RTOS/UI.
// All owning allocations in this module pass through this bounded budget.
struct Memory {
  static inline std::size_t used = 0, peak = 0;
  static inline std::size_t limit = 4 * 1024 * 1024;
  // Optional non-allocating observer, installed only by the serialized owner.
  // allocated=true with nullptr records a failed allocation; frees run before
  // releasing the block. This observes placement, never changes allocation.
  using Observer = void (*)(void *payload, std::size_t bytes, bool allocated);
  static inline Observer observer = nullptr;
  static void *allocate(std::size_t n) {
    if (used > limit || n > limit - used ||
        n > SIZE_MAX - sizeof(std::max_align_t)) {
      if (observer)
        observer(nullptr, n, true);
      return nullptr;
    }
    auto *p =
        static_cast<std::size_t *>(std::malloc(n + sizeof(std::max_align_t)));
    if (!p) {
      if (observer)
        observer(nullptr, n, true);
      return nullptr;
    }
    *p = n;
    used += n;
    peak = std::max(peak, used);
    void *payload = reinterpret_cast<char *>(p) + sizeof(std::max_align_t);
    if (observer)
      observer(payload, n, true);
    return payload;
  }
  static void release(void *p) {
    if (!p)
      return;
    auto *base = reinterpret_cast<std::size_t *>(static_cast<char *>(p) -
                                                 sizeof(std::max_align_t));
    if (observer)
      observer(p, *base, false);
    used -= *base;
    std::free(base);
  }
};
inline bool utf8(std::string_view s) {
  for (std::size_t i = 0; i < s.size();) {
    auto c = static_cast<unsigned char>(s[i++]);
    if (c < 128)
      continue;
    unsigned n = 0;
    std::uint32_t v = 0, min = 0;
    if ((c & 0xe0) == 0xc0) {
      n = 1;
      v = c & 31;
      min = 128;
    } else if ((c & 0xf0) == 0xe0) {
      n = 2;
      v = c & 15;
      min = 2048;
    } else if ((c & 0xf8) == 0xf0) {
      n = 3;
      v = c & 7;
      min = 65536;
    } else
      return false;
    if (n > s.size() - i)
      return false;
    while (n--) {
      auto d = static_cast<unsigned char>(s[i++]);
      if ((d & 0xc0) != 0x80)
        return false;
      v = (v << 6) | (d & 63);
    }
    if (v < min || v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff))
      return false;
  }
  return true;
}
class Text {
  struct Block {
    std::size_t refs, size;
  };
  Block *block_ = nullptr;
  const char *literal_ = "";
  std::size_t length_ = 0;
  bool valid_ = true;
  void drop() {
    if (block_ && !--block_->refs)
      Memory::release(block_);
    block_ = nullptr;
  }

public:
  Text() = default;
  explicit Text(std::string_view s) { assign(s); }
  static Text literal(const char *s) {
    Text t;
    t.literal_ = s;
    t.length_ = std::strlen(s);
    return t;
  }
  bool assign(std::string_view s) {
    if (s.size() > 4096 || !utf8(s)) {
      valid_ = false;
      return false;
    }
    auto *p =
        static_cast<Block *>(Memory::allocate(sizeof(Block) + s.size() + 1));
    if (!p) {
      valid_ = false;
      return false;
    }
    p->refs = 1;
    p->size = s.size();
    auto *d = reinterpret_cast<char *>(p + 1);
    std::memcpy(d, s.data(), s.size());
    d[s.size()] = 0;
    drop();
    block_ = p;
    length_ = s.size();
    valid_ = true;
    return true;
  }
  Text(const Text &t)
      : block_(t.block_), literal_(t.literal_), length_(t.length_),
        valid_(t.valid_) {
    if (block_)
      ++block_->refs;
  }
  Text(Text &&t) noexcept : Text() { swap(t); }
  Text &operator=(Text t) noexcept {
    swap(t);
    return *this;
  }
  void swap(Text &t) noexcept {
    std::swap(block_, t.block_);
    std::swap(literal_, t.literal_);
    std::swap(length_, t.length_);
    std::swap(valid_, t.valid_);
  }
  ~Text() { drop(); }
  bool valid() const { return valid_; }
  std::string_view view() const {
    return {block_ ? reinterpret_cast<const char *>(block_ + 1) : literal_,
            length_};
  }
  bool operator==(const Text &t) const {
    return valid_ == t.valid_ && view() == t.view();
  }
  bool operator<(const Text &t) const { return view() < t.view(); }
};
struct Id {
  Text text;
  Id() = default;
  explicit Id(std::string_view s) : text(s) {}
  bool valid() const {
    auto s = text.view();
    if (!text.valid() || s.empty() || s.size() > 128)
      return false;
    for (char c : s)
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == ':' ||
            c == '-'))
        return false;
    return true;
  }
  bool operator==(const Id &t) const { return text == t.text; }
  bool operator<(const Id &t) const { return text < t.text; }
};
template <class T, std::size_t N> class List {
  T *data_ = nullptr;
  std::size_t size_ = 0, capacity_ = 0;
  bool valid_ = true;
  bool grow(std::size_t c) {
    if (c > N)
      return false;
    auto *p = static_cast<T *>(Memory::allocate(sizeof(T) * c));
    if (!p)
      return false;
    for (std::size_t i = 0; i < size_; ++i) {
      new (p + i) T(std::move(data_[i]));
      data_[i].~T();
    }
    Memory::release(data_);
    data_ = p;
    capacity_ = c;
    return true;
  }

public:
  List() = default;
  List(const List &x) {
    if (!x.valid_) {
      valid_ = false;
      return;
    }
    for (const auto &v : x)
      if (!push(v)) {
        clear();
        valid_ = false;
        break;
      }
  }
  List(List &&x) noexcept { swap(x); }
  List &operator=(List x) noexcept {
    swap(x);
    return *this;
  }
  ~List() {
    clear();
    Memory::release(data_);
  }
  void swap(List &x) noexcept {
    std::swap(data_, x.data_);
    std::swap(size_, x.size_);
    std::swap(capacity_, x.capacity_);
    std::swap(valid_, x.valid_);
  }
  bool push(const T &v) {
    if (size_ == N)
      return false;
    if (size_ == capacity_ &&
        !grow(std::min(N, std::max(std::size_t(1), capacity_ * 2))))
      return false;
    new (data_ + size_) T(v);
    ++size_;
    return true;
  }
  bool push(T &&v) {
    if (size_ == N)
      return false;
    if (size_ == capacity_ &&
        !grow(std::min(N, std::max(std::size_t(1), capacity_ * 2))))
      return false;
    new (data_ + size_) T(std::move(v));
    ++size_;
    return true;
  }
  void erase(std::size_t i) {
    for (std::size_t j = i + 1; j < size_; ++j)
      data_[j - 1] = std::move(data_[j]);
    data_[--size_].~T();
  }
  void clear() {
    while (size_)
      data_[--size_].~T();
  }
  std::size_t size() const { return size_; }
  bool valid() const { return valid_; }
  T &operator[](std::size_t i) { return data_[i]; }
  const T &operator[](std::size_t i) const { return data_[i]; }
  T *begin() { return data_; }
  T *end() { return size_ ? data_ + size_ : data_; }
  const T *begin() const { return data_; }
  const T *end() const { return size_ ? data_ + size_ : data_; }
  bool operator==(const List &x) const {
    if (size_ != x.size_ || valid_ != x.valid_)
      return false;
    for (std::size_t i = 0; i < size_; ++i)
      if (!(data_[i] == x[i]))
        return false;
    return true;
  }
};
template <class T> struct Optional {
  bool present = false;
  T value{};
  Optional() = default;
  Optional(const T &v) : present(true), value(v) {}
  bool operator==(const Optional &) const = default;
};
enum class Absence { UNKNOWN, UNSUPPORTED, NOT_APPLICABLE, UNAVAILABLE };
template <class T> struct Value {
  bool present = false;
  T value{};
  Absence reason = Absence::UNKNOWN;
  static Value of(const T &t) {
    Value v;
    v.present = true;
    v.value = t;
    return v;
  }
  static Value absent(Absence r) {
    Value v;
    v.reason = r;
    return v;
  }
  bool operator==(const Value &x) const {
    return present == x.present &&
           (present ? value == x.value : reason == x.reason);
  }
};
} // namespace control
