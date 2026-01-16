use super::{frexp, log10u};
use std::collections::VecDeque;

// A module which represents a floating point value using base 1e9 digits,
// and tracks the radix point.

// We represent floating point values in base 1e9.
pub const DIGIT_WIDTH: usize = 9;
pub const DIGIT_BASE: u32 = 1_000_000_000;

// log2(1e9) = 29.9, so store 29 binary digits per base 1e9 decimal digit.
pub const BITS_PER_DIGIT: usize = 29;

// Returns n/d and n%d, rounding towards negative infinity.
#[inline]
pub fn divmod_floor(n: i32, d: i32) -> (i32, i32) {
    (n.div_euclid(d), n.rem_euclid(d))
}

// Helper to limit excess precision in our decimal representation.
// Do not compute more digits (in our base) than needed.
#[derive(Debug, Clone, Copy)]
pub enum DigitLimit {
    Total(usize),
    Fractional(usize),
}

// A struct representing an array of digits in base 1e9, along with the offset from the
// first digit to the least significant digit before the decimal, and the sign.
#[derive(Debug)]
pub struct Decimal {
    // The list of digits, in our base.
    pub digits: VecDeque<u32>,

    // The offset from first digit to least significant digit before the decimal.
    // Possibly negative!
    pub radix: i32,

    // Whether our initial value was negative.
    pub negative: bool,
}

impl Decimal {
    // Construct a Decimal from a floating point number.
    // The number must be finite.
    pub fn new(y: f64, limit: DigitLimit) -> Self {
        debug_assert!(y.is_finite());
        let negative = y.is_sign_negative();

        // Break the number into exponent and and mantissa.
        // Normalize mantissa to a single leading digit, if nonzero.
        let (mut y, mut e2) = frexp(y.abs());
        if y != 0.0 {
            y *= (1 << BITS_PER_DIGIT) as f64;
            e2 -= BITS_PER_DIGIT as i32;
        }

        // Express the mantissa as a decimal string in our base.
        let mut digits = Vec::new();
        while y != 0.0 {
            debug_assert!(y >= 0.0 && y < DIGIT_BASE as f64);
            let digit = y as u32;
            digits.push(digit);
            y = (DIGIT_BASE as f64) * (y - digit as f64);
        }

        // Construct ourselves and apply our exponent.
        let mut decimal = Decimal {
            digits: digits.into(),
            radix: 0,
            negative,
        };
        if e2 >= 0 {
            decimal.shift_left(e2 as usize);
        } else {
            decimal.shift_right(-e2 as usize, limit);
        }
        decimal
    }

    // Push a digit to the beginning, preserving the radix point.
    pub fn push_front(&mut self, digit: u32) {
        self.digits.push_front(digit);
        self.radix += 1;
    }

    // Push a digit to the end, preserving the radix point.
    pub fn push_back(&mut self, digit: u32) {
        self.digits.push_back(digit);
    }

    // Return the least significant digit.
    pub fn last(&self) -> Option<u32> {
        self.digits.back().copied()
    }

    // Return the most significant digit.
    pub fn first(&self) -> Option<u32> {
        self.digits.front().copied()
    }

    // Shift left by a power of 2.
    pub fn shift_left(&mut self, mut amt: usize) {
        while amt > 0 {
            let sh = amt.min(BITS_PER_DIGIT);
            let mut carry: u32 = 0;
            for digit in self.digits.iter_mut().rev() {
                let nd = ((*digit as u64) << sh) + carry as u64;
                *digit = (nd % DIGIT_BASE as u64) as u32;
                carry = (nd / DIGIT_BASE as u64) as u32;
            }
            if carry != 0 {
                self.push_front(carry);
            }
            self.trim_trailing_zeros();
            amt -= sh;
        }
    }

    // Shift right by a power of 2, limiting the precision.
    pub fn shift_right(&mut self, mut amt: usize, limit: DigitLimit) {
        // Divide by 2^sh, moving left to right.
        // Do no more than DIGIT_WIDTH at a time because that is the largest
        // power of 2 that divides DIGIT_BASE; therefore DIGIT_BASE >> sh
        // is always exact.
        while amt > 0 {
            let sh = amt.min(DIGIT_WIDTH);
            let mut carry: u32 = 0;
            // It is significantly faster to iterate over the two slices of the deque
            // than the deque itself.
            let (s1, s2) = self.digits.as_mut_slices();
            for digit in s1.iter_mut() {
                let remainder = *digit & ((1 << sh) - 1); // digit % 2^sh
                *digit = (*digit >> sh) + carry;
                carry = (DIGIT_BASE >> sh) * remainder;
            }
            for digit in s2.iter_mut() {
                let remainder = *digit & ((1 << sh) - 1); // digit % 2^sh
                *digit = (*digit >> sh) + carry;
                carry = (DIGIT_BASE >> sh) * remainder;
            }
            self.trim_leading_zeros();
            if carry != 0 {
                self.push_back(carry);
            }
            amt -= sh;
            // Truncate if we have computed more than we need.
            match limit {
                DigitLimit::Total(n) => {
                    self.digits.truncate(n);
                }
                DigitLimit::Fractional(n) => {
                    let current = (self.digits.len() as i32 - self.radix - 1).max(0) as usize;
                    let to_trunc = current.saturating_sub(n);
                    self.digits
                        .truncate(self.digits.len().saturating_sub(to_trunc));
                }
            }
        }
    }

    // Return the length as an i32.
    pub fn len_i32(&self) -> i32 {
        self.digits.len() as i32
    }

    // Compute the exponent, base 10.
    pub fn exponent(&self) -> i32 {
        let Some(first_digit) = self.first() else {
            return 0;
        };
        self.radix * (DIGIT_WIDTH as i32) + log10u(first_digit)
    }

    // Compute the number of fractional digits - possibly negative.
    pub fn fractional_digit_count(&self) -> i32 {
        (DIGIT_WIDTH as i32) * (self.digits.len() as i32 - self.radix - 1)
    }

    // Trim leading zeros.
    fn trim_leading_zeros(&mut self) {
        while self.digits.front() == Some(&0) {
            self.digits.pop_front();
            self.radix -= 1;
        }
    }

    // Trim trailing zeros.
    fn trim_trailing_zeros(&mut self) {
        while self.digits.iter().last() == Some(&0) {
            self.digits.pop_back();
        }
    }

    // Round to a given number of fractional digits (possibly negative).
    pub fn round_to_fractional_digits(&mut self, desired_frac_digits: i32) {
        let frac_digit_count = self.fractional_digit_count();
        if desired_frac_digits >= frac_digit_count {
            return;
        }
        let (quot, rem) = divmod_floor(desired_frac_digits, DIGIT_WIDTH as i32);
        // Find the index of the last digit to keep.
        let mut last_digit_idx = self.radix + 1 + quot;

        // If desired_frac_digits is small, and we are very small, then last_digit_idx may be negative.
        while last_digit_idx < 0 {
            self.push_front(0);
            last_digit_idx += 1;
        }

        // Now we have the index of the digit - figure out how much of the digit to keep.
        // If 'rem' is 0 then we keep all of it; if 'rem' is 8 then we keep only the most
        // significant power of 10 (mod by 10**8).
        debug_assert!(DIGIT_WIDTH as i32 > rem);
        let mod_base = 10u32.pow((DIGIT_WIDTH as i32 - rem) as u32);
        debug_assert!(mod_base <= DIGIT_BASE);

        let remainder_to_round = self[last_digit_idx] % mod_base;
        self[last_digit_idx] -= remainder_to_round;

        // Round up if necessary.
        if self.should_round_up(last_digit_idx, remainder_to_round, mod_base) {
            self[last_digit_idx] += mod_base;
            // Propagate carry.
            while self[last_digit_idx] >= DIGIT_BASE {
                self[last_digit_idx] = 0;
                last_digit_idx -= 1;
                if last_digit_idx < 0 {
                    self.push_front(0);
                    last_digit_idx = 0;
                }
                self[last_digit_idx] += 1;
            }
        }
        self.digits.truncate(last_digit_idx as usize + 1);
        self.trim_trailing_zeros();
    }

    // We are about to round ourself such that digit_idx is the last digit,
    // with mod_base being a power of 10 such that we round off self[digit_idx] % mod_base,
    // which is given by remainder (that is, remainder = self[digit_idx] % mod_base).
    // Return true if we should round up (in magnitude), as determined by the floating point
    // rounding mode.
    #[inline]
    fn should_round_up(&self, digit_idx: i32, remainder: u32, mod_base: u32) -> bool {
        if remainder == 0 && digit_idx + 1 == self.len_i32() {
            // No remaining digits.
            return false;
        }

        // 'round' is the first float such that 'round + 1.0' is not representable.
        // We will add a value to it and see whether it rounds up or down, thus
        // matching the fp rounding mode.
        let mut round = 2.0_f64.powi(f64::MANTISSA_DIGITS as i32);

        // In the likely event that the fp rounding mode is FE_TONEAREST, then ties are rounded to
        // the nearest value with a least significant digit of 0. Ensure 'round's least significant
        // bit agrees with whether our rounding digit is odd.
        let rounding_digit = if mod_base < DIGIT_BASE {
            self[digit_idx] / mod_base
        } else if digit_idx > 0 {
            self[digit_idx - 1]
        } else {
            0
        };
        if rounding_digit & 1 != 0 {
            round += 2.0;
            // round now has an odd lsb (though round itself is even).
            debug_assert_ne!(round.to_bits() & 1, 0);
        }

        // Set 'small' to a value which is less than halfway, exactly halfway, or more than halfway
        // between round and the next representable float (which is round + 2.0).
        let mut small = if remainder < mod_base / 2 {
            0.5
        } else if remainder == mod_base / 2 && digit_idx + 1 == self.len_i32() {
            1.0
        } else {
            1.5
        };

        // If the initial value was negative, then negate round and small, thus respecting FE_UPWARD / FE_DOWNWARD.
        if self.negative {
            round = -round;
            small = -small;
        }

        // Round up if round + small increases (in magnitude).
        round + small != round
    }
}

// Index, with i32.
impl std::ops::Index<i32> for Decimal {
    type Output = u32;

    fn index(&self, index: i32) -> &Self::Output {
        assert!(index >= 0);
        &self.digits[index as usize]
    }
}

// IndexMut, with i32.
impl std::ops::IndexMut<i32> for Decimal {
    fn index_mut(&mut self, index: i32) -> &mut Self::Output {
        assert!(index >= 0);
        &mut self.digits[index as usize]
    }
}
