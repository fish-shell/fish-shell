use super::*;
use decimal::*;
use std::collections::VecDeque;

#[test]
fn test_frexp() {
    // Note f64::MIN_POSITIVE is normalized - we want denormal.
    let min_pos_denormal = f64::from_bits(1);
    let min_neg_denormal = -min_pos_denormal;
    let cases = vec![
        (0.0, (0.0, 0)),
        (-0.0, (-0.0, 0)),
        (1.0, (0.5, 1)),
        (-1.0, (-0.5, 1)),
        (2.5, (0.625, 2)),
        (-2.5, (-0.625, 2)),
        (1024.0, (0.5, 11)),
        (f64::MAX, (0.9999999999999999, 1024)),
        (-f64::MAX, (-0.9999999999999999, 1024)),
        (f64::INFINITY, (f64::INFINITY, 0)),
        (f64::NEG_INFINITY, (f64::NEG_INFINITY, 0)),
        (f64::NAN, (f64::NAN, 0)),
        (min_pos_denormal, (0.5, -1073)),
        (min_neg_denormal, (-0.5, -1073)),
    ];

    for (x, (want_frac, want_exp)) in cases {
        let (frac, exp) = frexp(x);
        if x.is_nan() {
            assert!(frac.is_nan());
            continue;
        }
        assert_eq!(frac, want_frac);
        assert_eq!(frac.is_sign_negative(), want_frac.is_sign_negative());
        assert_eq!(exp, want_exp);
    }
}

#[test]
fn test_log10u() {
    assert_eq!(log10u(0), 0);
    assert_eq!(log10u(1), 0);
    assert_eq!(log10u(5), 0);
    assert_eq!(log10u(9), 0);
    assert_eq!(log10u(10), 1);
    assert_eq!(log10u(500), 2);
    assert_eq!(log10u(6000), 3);
    assert_eq!(log10u(9999), 3);
    assert_eq!(log10u(70000), 4);
    assert_eq!(log10u(70001), 4);
    assert_eq!(log10u(900000), 5);
    assert_eq!(log10u(3000000), 6);
    assert_eq!(log10u(50000000), 7);
    assert_eq!(log10u(100000000), 8);
    assert_eq!(log10u(1840683745), 9);
    assert_eq!(log10u(4000000000), 9);
    assert_eq!(log10u(u32::MAX), 9);
}

#[test]
fn test_div_floor() {
    for numer in -100..100 {
        for denom in 1..100 {
            let (q, r) = divmod_floor(numer, denom);
            assert!(r >= 0, "Remainder should be non-negative");
            assert!(r < denom.abs(), "Remainder should be less than divisor");
            assert_eq!(numer, q * denom + r, "Quotient should be exact");
        }
    }
    assert_eq!(divmod_floor(i32::MIN, 1), (i32::MIN, 0));
    assert_eq!(divmod_floor(i32::MIN, i32::MAX), (-2, i32::MAX - 1));
    assert_eq!(divmod_floor(i32::MAX, i32::MAX), (1, 0));
}

#[test]
fn test_digits_new() {
    let unlimit = DigitLimit::Total(usize::MAX);
    let mut decimal = Decimal::new(0.0, unlimit);
    assert_eq!(decimal.digits, &[]);
    assert_eq!(decimal.radix, 0);

    decimal = Decimal::new(1.0, unlimit);
    assert_eq!(decimal.digits, &[1]);
    assert_eq!(decimal.radix, 0);

    decimal = Decimal::new(0.5, unlimit);
    assert_eq!(decimal.digits, &[500_000_000]);
    assert_eq!(decimal.radix, -1);

    decimal = Decimal::new(0.25, unlimit);
    assert_eq!(decimal.digits, &[250_000_000]);
    assert_eq!(decimal.radix, -1);

    decimal = Decimal::new(2.0, unlimit);
    assert_eq!(decimal.digits, &[2]);
    assert_eq!(decimal.radix, 0);

    decimal = Decimal::new(1_234_567_890.5, unlimit);
    assert_eq!(decimal.digits, &[1, 234_567_890, 500_000_000]);
    assert_eq!(decimal.radix, 1);

    decimal = Decimal::new(12_345_678_901.0, unlimit);
    assert_eq!(decimal.digits, &[12, 345_678_901]);
    assert_eq!(decimal.radix, 1);

    decimal = Decimal::new(2.0_f64.powi(-1), unlimit);
    assert_eq!(decimal.digits, &[500_000_000]);
    assert_eq!(decimal.radix, -1);

    decimal = Decimal::new(2.0_f64.powi(-2), unlimit);
    assert_eq!(decimal.digits, &[250_000_000]);
    assert_eq!(decimal.radix, -1);

    decimal = Decimal::new(2.0_f64.powi(-4), unlimit);
    assert_eq!(decimal.digits, &[62_500_000]);
    assert_eq!(decimal.radix, -1);

    decimal = Decimal::new(2.0_f64.powi(-8), unlimit);
    assert_eq!(decimal.digits, &[3_906_250]);
    assert_eq!(decimal.radix, -1);

    decimal = Decimal::new(2.0_f64.powi(-16), unlimit);
    assert_eq!(decimal.digits, &[15_258, 789_062_500]);
    assert_eq!(decimal.radix, -1);

    decimal = Decimal::new(2.0_f64.powi(-64), unlimit);
    assert_eq!(
        decimal.digits,
        &[
            54_210_108,
            624_275_221,
            700_372_640,
            43_497_085,
            571_289_062,
            500_000_000
        ]
    );
    assert_eq!(decimal.radix, -3);

    assert!(!Decimal::new(1.0, unlimit).negative);
    assert!(Decimal::new(-1.0, unlimit).negative);
    assert!(!Decimal::new(0.0, unlimit).negative);
    assert!(Decimal::new(-0.0, unlimit).negative);
}

#[test]
fn test_shift_left() {
    // No carry.
    let mut decimal = Decimal {
        digits: VecDeque::from(vec![1, 2]),
        radix: 0,
        negative: false,
    };
    decimal.shift_left(1);
    assert_eq!(decimal.digits, &[2, 4]);
    assert_eq!(decimal.radix, 0);

    // Simple carry. Trailing zeros are trimmed.
    let mut decimal = Decimal {
        digits: VecDeque::from(vec![500_000_000, 500_000_000]),
        radix: 0,
        negative: false,
    };
    decimal.shift_left(1);
    assert_eq!(decimal.digits, &[1, 1]);
    assert_eq!(decimal.radix, 1);

    // Big carry.
    // 1 << 100 == 1267650600228229401496703205376
    let mut decimal = Decimal {
        digits: VecDeque::from(vec![1]),
        radix: 0,
        negative: false,
    };
    decimal.shift_left(100);
    assert_eq!(
        decimal.digits,
        &[1267, 650_600_228, 229_401_496, 703_205_376]
    );
    assert_eq!(decimal.radix, 3);
}

#[test]
fn test_shift_right() {
    let unlimit = DigitLimit::Total(usize::MAX);
    // No carry.
    let mut decimal = Decimal {
        digits: VecDeque::from(vec![2, 4]),
        radix: 0,
        negative: false,
    };
    decimal.shift_right(1, unlimit);
    assert_eq!(decimal.digits, &[1, 2]);
    assert_eq!(decimal.radix, 0);

    // Carry. Leading zeros are trimmed.
    let mut decimal = Decimal {
        digits: VecDeque::from(vec![1, 0, 0]),
        radix: 1,
        negative: false,
    };
    decimal.shift_right(1, unlimit);
    assert_eq!(decimal.digits, &[500_000_000, 0]);
    assert_eq!(decimal.radix, 0);

    // Big shift right
    // 1267650600228229401496703205376 >> 100 should logically result in 1
    let mut decimal = Decimal {
        digits: VecDeque::from(vec![1_267, 650_600_228, 229_401_496, 703_205_376]),
        radix: 3,
        negative: false,
    };
    decimal.shift_right(100, unlimit);
    assert_eq!(decimal.digits, VecDeque::from(vec![1]));
    assert_eq!(decimal.radix, 0);
}

#[test]
fn test_shift_right_with_precision() {
    let mut decimal = Decimal {
        digits: VecDeque::from(vec![1]),
        radix: 1,
        negative: false,
    };
    decimal.shift_right(10, DigitLimit::Total(1));
    assert_eq!(decimal.digits, &[976562]);
    assert_eq!(decimal.radix, 0);

    decimal = Decimal {
        digits: VecDeque::from(vec![10000000, 10000000, 0]),
        radix: 3,
        negative: false,
    };
    decimal.shift_right(10, DigitLimit::Total(3));
    assert_eq!(decimal.digits, &[9765, 625009765, 625000000]);
    assert_eq!(decimal.radix, 3);

    let mut decimal = Decimal {
        digits: VecDeque::from(vec![1]),
        radix: 1,
        negative: false,
    };
    decimal.shift_right(10, DigitLimit::Fractional(1));
    assert_eq!(decimal.digits, &[976562, 500000000]);
    assert_eq!(decimal.radix, 0);
    decimal.shift_right(20, DigitLimit::Fractional(1));
    assert_eq!(decimal.digits, &[931322574]);
    assert_eq!(decimal.radix, -1);

    decimal = Decimal {
        digits: VecDeque::from(vec![10000000, 10000000, 0]),
        radix: 3,
        negative: false,
    };
    decimal.shift_right(10, DigitLimit::Total(3));
    assert_eq!(decimal.digits, &[9765, 625009765, 625000000]);
    assert_eq!(decimal.radix, 3);
}

#[test]
fn test_exponent() {
    let decimal = Decimal {
        digits: VecDeque::from(vec![123456789]),
        radix: 2,
        negative: false,
    };
    assert_eq!(decimal.exponent(), 2 * (DIGIT_WIDTH as i32) + 8);

    let decimal = Decimal {
        digits: VecDeque::from(vec![12345]),
        radix: -1,
        negative: false,
    };
    assert_eq!(decimal.exponent(), -(DIGIT_WIDTH as i32) + 4);

    let decimal = Decimal {
        digits: VecDeque::from(vec![123456789]),
        radix: 0,
        negative: false,
    };
    assert_eq!(decimal.exponent(), 8);

    let decimal = Decimal {
        digits: VecDeque::new(),
        radix: 0,
        negative: false,
    };
    assert_eq!(decimal.exponent(), 0);
}
