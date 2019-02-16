#pragma once

#include <bitset>
#include <cassert>
#include <iterator>

/// A type (to specialize) that provides a count for an enum.
/// Example:
///   template<> struct enum_info_t<MyEnum>
///   { static constexpr auto count = MyEnum::COUNT; };
template <typename T>
struct enum_info_t {};

template <typename T>
class enum_set_t : private std::bitset<static_cast<size_t>(enum_info_t<T>::count)> {
   private:
    using super = std::bitset<static_cast<size_t>(enum_info_t<T>::count)>;
    static size_t index_of(T t) { return static_cast<size_t>(t); }

    explicit enum_set_t(unsigned long raw) : super(raw) {}

   public:
    enum_set_t() = default;

    explicit enum_set_t(T v) { set(v); }

    static enum_set_t from_raw(unsigned long v) { return enum_set_t{v}; }

    unsigned long to_raw() const { return super::to_ulong(); }

    bool get(T t) const { return super::test(index_of(t)); }

    void set(T t, bool v = true) { super::set(index_of(t), v); }

    void clear(T t) { super::reset(index_of(t)); }

    bool none() const { return super::none(); }

    bool any() const { return super::any(); }

    bool operator==(const enum_set_t &rhs) const { return super::operator==(rhs); }

    bool operator!=(const enum_set_t &rhs) const { return super::operator!=(rhs); }
};

/// A counting iterator for an enum class.
/// This enumerates the values of an enum class from 0 up to (not including) count.
/// Example:
///   for (auto v : enum_iter_t<MyEnum>) {...}
template <typename T>
class enum_iter_t {
    using base_type_t = typename std::underlying_type<T>::type;
    struct iterator_t {
        friend class enum_iter_t;
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = long;

        explicit iterator_t(base_type_t v) : v_(v) {}

        T operator*() const { return static_cast<T>(v_); }
        const T *operator->() const { return static_cast<const T *>(v_); }

        iterator_t &operator++() {
            v_ += 1;
            return *this;
        }

        iterator_t operator++(int) {
            auto res = *this;
            v_ += 1;
            return res;
        }

        bool operator==(iterator_t rhs) const { return v_ == rhs.v_; }
        bool operator!=(iterator_t rhs) const { return v_ != rhs.v_; }

       private:
        base_type_t v_{};
    };

   public:
    iterator_t begin() const { return iterator_t{0}; }
    iterator_t end() const { return iterator_t{static_cast<base_type_t>(enum_info_t<T>::count)}; }
};
