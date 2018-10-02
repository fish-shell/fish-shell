#pragma once

#include <bitset>

template <typename T>
class enum_set_t {
   private:
    using base_type_t = typename std::underlying_type<T>::type;
    std::bitset<8 * sizeof(base_type_t)> bitmask{0};
    static int index_of(T t) { return static_cast<base_type_t>(t); }

   public:
    bool get(T t) const { return bitmask.test(index_of(t)); }

    void set(T t, bool v) {
        if (v) {
            bitmask.set(index_of(t));
        } else {
            bitmask.reset(index_of(t));
        }
    }

    void set(T t) { bitmask.set(index_of(t)); }

    void clear(T t) { bitmask.reset(index_of(t)); }
};
