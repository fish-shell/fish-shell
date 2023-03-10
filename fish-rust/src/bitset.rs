use num_traits::{AsPrimitive, FromPrimitive, PrimInt};
use std::ops::{BitAndAssign, BitOrAssign};

pub struct BitSet<T>(T);

impl<T> BitSet<T>
where
    T: PrimInt + BitAndAssign<T> + BitOrAssign<T> + FromPrimitive + AsU8 + AsI64 + AsU64,
    i64: AsPrimitive<T>,
{
    /// Set's the `i`th bit to `1`.
    pub fn set(&mut self, i: usize) {
        self.0 |= T::one() << i;
    }

    /// Clears the `i`th bit (i.e. sets it to `0`).
    pub fn unset(&mut self, i: usize) {
        self.0 &= !(T::one() << i);
    }

    /// Sets or clears the `i`th bit depending on the value of `v`.
    ///
    /// Equivalent to the following:
    ///
    /// ```no_run
    /// let bitset = BitSet::new(0u64);
    /// let v = todo!();
    /// if v { bitset.set(i) } else { bitset.unset(i) }
    /// ```
    ///
    /// except it is executed branchlessly.
    pub fn toggle(&mut self, i: usize, v: bool) {
        let v = T::from_u8(v as u8).unwrap();
        let mask = T::one() << i;
        let bit: T = (-v.as_i64()).as_();
        self.0 = (self.0 & !mask) | (bit & mask);
    }

    /// Clears all the bits in the `BitSet`.
    pub fn clear(&mut self) {
        self.0 = T::zero();
    }

    /// Tests whether the `i`th bit is set.
    pub fn test(&self, i: usize) -> bool {
        ((self.0 >> i).as_u8() & 0x01) == 0x01
    }

    /// If `i` is within `BitSet::size()`, returns whether or not the `i`th bit is set. If `i` is
    /// greater than the size of the bitset, returns `None`.
    pub fn get(&self, i: usize) -> Option<bool> {
        if i >= Self::size() {
            None
        } else {
            Some((self.0 >> i).as_u8() == 0x01)
        }
    }

    /// Returns the maximum size of the `BitSet` (in bits). A `BitSet` does not have a separate
    /// count; the size is both the number of elements that a `BitSet` contains and the maximum
    /// number of elements/bits that it can contain.
    ///
    /// This value is fixed dependent on the underlying integral type and cannot change.
    pub fn size() -> usize {
        T::max_value().count_ones() as usize
    }

    /// Returns the number of bits set in the `BitSet`.
    pub fn count(&self) -> usize {
        self.0.count_ones() as usize
    }

    /// Returns `true` if all the bits in the `BitSet` are not set.
    pub fn is_empty(&self) -> bool {
        self.0.is_zero()
    }

    /// Iterates over all the bits in the `BitSet` starting with the LSB.
    pub fn iter(&self) -> IterBits {
        let size = Self::size();
        let value = self.0.as_u64().rotate_right(size as u32);
        IterBits {
            value,
            offset: 64 - size as u8,
        }
    }

    /// Iterates over the indices of the bits that have been set in the `BitSet` starting with the
    /// LSB.
    pub fn iter_set_bits(&self) -> IterSetBits {
        IterSetBits {
            value: self.0.as_u64(),
        }
    }
}

/// Iterates over all the bits in a [`BitSet`]. Not to be used directly, see [`BitSet::iter()`].
// Note: this structure is hard-coded to go through a u64 for simplicity (and since the size doesn't
// matter as it's likely a transient object and not being stored). If there's a need to make a
// `BitSet<u128>` some day, this should not be changed to use u128 internally but rather should be
// refactored to use the same `T` instead (as u128 is much slower than u64).
pub struct IterBits {
    offset: u8,
    value: u64,
}

/// Iterates over all the indices of set bits in a [`BitSet`]. Not to be used directly, see
/// [`BitSet::iter_set_bits()`].
// Note: this structure is hard-coded to go through a u64 for simplicity (and since the size doesn't
// matter as it's likely a transient object and not being stored). If there's a need to make a
// `BitSet<u128>` some day, this should not be changed to use u128 internally but rather should be
// refactored to use the same `T` instead (as u128 is much slower than u64).
pub struct IterSetBits {
    value: u64,
}

impl Iterator for IterBits {
    type Item = bool;

    fn next(&mut self) -> Option<Self::Item> {
        if self.offset == 64 {
            return None;
        }
        let value = (self.value >> self.offset) & 0x01;
        self.offset += 1;
        Some(value != 0)
    }
}

impl Iterator for IterSetBits {
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        let offset = self.value.trailing_zeros();
        if offset == 64 {
            return None;
        }

        self.value &= !(1 << offset);
        return Some(offset as usize);
    }
}

/// Trait to cast a numeric type `T` to a `u8`.
///
/// Convenience trait for [`AsPrimitive<u8>`], since the [`AsPrimitive::as_()`] function name is
/// shared with all the other `AsPrimitive<T>` variants, making it clearer what `as_()` is supposed
/// to do and letting us use multiple `AsPrimitive<X>` without needing to use the obtuse `<T as
/// AsPrimitive<X>>::as_(self.0)` syntax.
pub trait AsU8: 'static + Copy {
    fn as_u8(&self) -> u8;
}

impl<T: AsPrimitive<u8>> AsU8 for T {
    fn as_u8(&self) -> u8 {
        self.as_()
    }
}

/// Trait to cast a numeric type `T` to a `u64`.
///
/// Convenience trait for [`AsPrimitive<u64>`], since the [`AsPrimitive::as_()`] function name is
/// shared with all the other `AsPrimitive<T>` variants, making it clearer what `as_()` is supposed
/// to do and letting us use multiple `AsPrimitive<X>` without needing to use the obtuse `<T as
/// AsPrimitive<X>>::as_(self.0)` syntax.
pub trait AsU64: 'static + Copy {
    fn as_u64(&self) -> u64;
}

impl<T: AsPrimitive<u64>> AsU64 for T {
    fn as_u64(&self) -> u64 {
        self.as_()
    }
}

/// Trait to cast a numeric type `T` to a `i64`.
///
/// Convenience trait for [`AsPrimitive<i64>`], since the [`AsPrimitive::as_()`] function name is
/// shared with all the other `AsPrimitive<T>` variants, making it clearer what `as_()` is supposed
/// to do and letting us use multiple `AsPrimitive<X>` without needing to use the obtuse `<T as
/// AsPrimitive<X>>::as_(self.0)` syntax.
pub trait AsI64: 'static + Copy {
    fn as_i64(&self) -> i64;
}

impl<T: AsPrimitive<i64>> AsI64 for T {
    fn as_i64(&self) -> i64 {
        self.as_()
    }
}

impl<T: PrimInt> Default for BitSet<T> {
    fn default() -> Self {
        BitSet(T::zero())
    }
}

impl BitSet<u8> {
    pub const fn new() -> Self {
        Self(0)
    }
}

impl BitSet<u16> {
    pub const fn new() -> Self {
        Self(0)
    }
}

impl BitSet<u32> {
    pub const fn new() -> Self {
        Self(0)
    }
}

impl BitSet<u64> {
    pub const fn new() -> Self {
        Self(0)
    }
}

#[test]
fn test_size() {
    assert_eq!(BitSet::<u8>::size(), 8);
    assert_eq!(BitSet::<u32>::size(), 32);
}

#[test]
fn test_set() {
    let mut bitset = BitSet::<u32>::new();
    assert!(!bitset.test(18));
    bitset.set(18);
    assert!(bitset.test(18));
}

#[test]
fn test_unset() {
    let mut bitset = BitSet::<u32>::new();
    bitset.set(18);
    bitset.unset(18);
    assert!(!bitset.test(18))
}

#[test]
fn test_empty() {
    let mut bitset = BitSet::<u16>::new();
    assert!(bitset.is_empty());
    bitset.set(8);
    assert!(!bitset.is_empty());
}

#[test]
fn test_get() {
    let mut bitset = BitSet::<u32>::new();
    assert!(bitset.get(0).is_some());
    assert!(bitset.get(18).is_some());
    assert!(bitset.get(32).is_none());
    assert!(bitset.get(33).is_none());

    bitset.set(14);
    assert_eq!(bitset.get(14), Some(true));
    assert_eq!(bitset.get(15), Some(false));

    // A test for platforms where usize is less than u64
    let bitset = BitSet::<u64>::new();
    assert!(bitset.get(1).is_some());
    assert!(bitset.get(64).is_none());
}

#[test]
fn test_clear() {
    let mut bitset = BitSet::<u64>::new();
    bitset.set(11);
    assert!(!bitset.is_empty());
    bitset.clear();
    assert!(bitset.is_empty());
}

#[test]
fn test_toggle() {
    let mut bitset = BitSet::<u64>::new();
    bitset.toggle(12, false);
    assert_eq!(bitset.get(12), Some(false));
    bitset.toggle(12, true);
    assert_eq!(bitset.get(12), Some(true));
}

#[test]
fn test_iter_set() {
    let mut bitset = BitSet::<u8>::new();
    assert_eq!(bitset.iter_set_bits().collect::<Vec<_>>(), Vec::new());
    bitset.set(0);
    bitset.set(5);
    bitset.set(3);
    bitset.set(7);
    let mut iter = bitset.iter_set_bits();
    assert_eq!(iter.next(), Some(0));
    assert_eq!(iter.next(), Some(3));
    assert_eq!(iter.next(), Some(5));
    assert_eq!(iter.next(), Some(7));
    assert_eq!(iter.next(), None);
    assert_eq!(iter.next(), None);
}

#[test]
fn test_iter() {
    let mut bitset = BitSet::<u8>::new();
    assert_eq!(&bitset.iter().collect::<Vec<_>>(), &[false; 8]);
    bitset.set(0);
    bitset.set(5);
    bitset.set(3);
    let mut iter = bitset.iter();
    assert_eq!(iter.next(), Some(true));
    assert_eq!(iter.next(), Some(false));
    assert_eq!(iter.next(), Some(false));
    assert_eq!(iter.next(), Some(true));
    assert_eq!(iter.next(), Some(false));
    assert_eq!(iter.next(), Some(true));
    assert_eq!(iter.next(), Some(false));
    assert_eq!(iter.next(), Some(false));
    assert_eq!(iter.next(), None);
    assert_eq!(iter.next(), None);
}
