#ifndef FISH_ENUM_SET_H
#define FISH_ENUM_SET_H

#include <array>
#include <bitset>
#include <cassert>
#include <iterator>

/// A type (to specialize) that provides a count for an enum.
/// Example:
///   template<> struct enum_info_t<MyEnum>
///   { static constexpr auto count = MyEnum::COUNT; };
template <typename T>
struct enum_info_t {};

/// \return the count of an enum.
template <typename T>
constexpr size_t enum_count() {
    return static_cast<size_t>(enum_info_t<T>::count);
}

/// A bit set indexed by an enum type.
template <typename T>
class enum_set_t : private std::bitset<enum_count<T>()> {
   private:
    using super = std::bitset<enum_count<T>()>;
    static size_t index_of(T t) { return static_cast<size_t>(t); }

    explicit enum_set_t(unsigned long raw) : super(raw) {}
    explicit enum_set_t(super sup) : super(std::move(sup)) {}

   public:
    enum_set_t() = default;

    /*implicit*/ enum_set_t(T v) { set(v); }

    /*implicit*/ enum_set_t(std::initializer_list<T> vs) {
        for (T v : vs) set(v);
    }

    static enum_set_t from_raw(unsigned long v) { return enum_set_t{v}; }

    unsigned long to_raw() const { return super::to_ulong(); }

    bool get(T t) const { return super::test(index_of(t)); }

    void set(T t, bool v = true) { super::set(index_of(t), v); }

    void clear(T t) { super::reset(index_of(t)); }

    bool none() const { return super::none(); }

    bool any() const { return super::any(); }

    bool operator==(const enum_set_t &rhs) const { return super::operator==(rhs); }

    bool operator!=(const enum_set_t &rhs) const { return super::operator!=(rhs); }

    /// OR in a single flag, returning a new set.
    enum_set_t operator|(T rhs) const {
        enum_set_t result = *this;
        result.set(rhs);
        return result;
    }

    /// Compute the union of two sets.
    enum_set_t operator|(enum_set_t rhs) const { return from_raw(to_raw() | rhs.to_raw()); }

    /// OR in a single flag, modifying the set in place.
    enum_set_t operator|=(T rhs) {
        *this = *this | rhs;
        return *this;
    }

    /// Set this to the union of two sets.
    enum_set_t operator|=(enum_set_t rhs) {
        *this = *this | rhs;
        return *this;
    }

    /// Test a value of a single flag. Note this does not return an enum_set_t; there is no such
    /// boolean conversion. This simply makes flags work more naturally as bit masks.
    bool operator&(T rhs) const { return get(rhs); }
};

/// An array of Elem indexed by an enum class.
template <typename Elem, typename T>
class enum_array_t : public std::array<Elem, enum_count<T>()> {
    using super = std::array<Elem, enum_count<T>()>;
    using base_type_t = typename std::underlying_type<T>::type;

    static int index_of(T t) { return static_cast<base_type_t>(t); }

   public:
    Elem &at(T t) { return super::at(index_of(t)); }
    const Elem &at(T t) const { return super::at(index_of(t)); }
    Elem &operator[](T t) { return super::operator[](index_of(t)); }
    const Elem &operator[](T t) const { return super::operator[](index_of(t)); }
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
    iterator_t end() const { return iterator_t{static_cast<base_type_t>(enum_count<T>())}; }
};

#endif
