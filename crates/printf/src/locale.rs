/// The numeric locale. Note this is a pure value type.
#[derive(Debug, Clone, Copy)]
pub struct Locale {
    /// The decimal point. Only single-char decimal points are supported.
    pub decimal_point: char,

    /// The thousands separator, or None if none.
    /// Note some obscure locales like it_IT.ISO8859-15 seem to have a multi-char thousands separator!
    /// We do not support that.
    pub thousands_sep: Option<char>,

    /// The grouping of digits.
    /// This is to be read from left to right.
    /// For example, the number 88888888888888 with a grouping of [2, 3, 4, 4]
    /// would produce the string "8,8888,8888,888,88".
    /// If 0, no grouping at all.
    pub grouping: [u8; 4],

    /// If true, the group is repeated.
    /// If false, there are no groups after the last.
    pub group_repeat: bool,
}

impl Locale {
    /// Given a string containing only ASCII digits, return a new string with thousands separators applied.
    /// This panics if the locale has no thousands separator; callers should only call this if there is a
    /// thousands separator.
    pub fn apply_grouping(&self, mut input: &str) -> String {
        debug_assert!(input.bytes().all(|b| b.is_ascii_digit()));
        let sep = self.thousands_sep.expect("no thousands separator");
        let mut result = String::with_capacity(input.len() + self.separator_count(input.len()));
        while !input.is_empty() {
            let group_size = self.next_group_size(input.len());
            let (group, rest) = input.split_at(group_size);
            result.push_str(group);
            if !rest.is_empty() {
                result.push(sep);
            }
            input = rest;
        }
        result
    }

    // Given a count of remaining digits, return the number of characters in the next group, from the left (most significant).
    fn next_group_size(&self, digits_left: usize) -> usize {
        let mut accum: usize = 0;
        for group in self.grouping {
            if digits_left <= accum + group as usize {
                return digits_left - accum;
            }
            accum += group as usize;
        }
        // accum now contains the sum of all groups.
        // Maybe repeat.
        debug_assert!(digits_left >= accum);
        let repeat_group = if self.group_repeat {
            *self.grouping.last().unwrap()
        } else {
            0
        };

        if repeat_group == 0 {
            // No further grouping.
            digits_left - accum
        } else {
            // Divide remaining digits by repeat_group.
            // Apply any remainder to the first group.
            let res = (digits_left - accum) % (repeat_group as usize);
            if res > 0 { res } else { repeat_group as usize }
        }
    }

    // Given a count of remaining digits, return the total number of separators.
    pub fn separator_count(&self, digits_count: usize) -> usize {
        if self.thousands_sep.is_none() {
            return 0;
        }
        let mut sep_count = 0;
        let mut accum = 0;
        for group in self.grouping {
            if digits_count <= accum + group as usize {
                return sep_count;
            }
            if group > 0 {
                sep_count += 1;
            }
            accum += group as usize;
        }
        debug_assert!(digits_count >= accum);
        let repeat_group = if self.group_repeat {
            *self.grouping.last().unwrap()
        } else {
            0
        };
        // Divide remaining digits by repeat_group.
        // -1 because it's "100,000" and not ",100,100".
        if repeat_group > 0 && digits_count > accum {
            sep_count += (digits_count - accum - 1) / repeat_group as usize;
        }
        sep_count
    }
}

/// The "C" numeric locale.
pub const C_LOCALE: Locale = Locale {
    decimal_point: '.',
    thousands_sep: None,
    grouping: [0; 4],
    group_repeat: false,
};

// en_us numeric locale, for testing.
#[allow(dead_code)]
pub const EN_US_LOCALE: Locale = Locale {
    decimal_point: '.',
    thousands_sep: Some(','),
    grouping: [3, 3, 3, 3],
    group_repeat: true,
};

#[test]
fn test_apply_grouping() {
    let input = "123456789";
    let mut result: String;

    // en_US has commas.
    assert_eq!(EN_US_LOCALE.thousands_sep, Some(','));
    result = EN_US_LOCALE.apply_grouping(input);
    assert_eq!(result, "123,456,789");

    // Test weird locales.
    let input: &str = "1234567890123456";
    let mut locale: Locale = C_LOCALE;
    locale.thousands_sep = Some('!');

    locale.grouping = [5, 3, 1, 0];
    locale.group_repeat = false;
    result = locale.apply_grouping(input);
    assert_eq!(result, "1234567!8!901!23456");

    // group_repeat doesn't matter because trailing group is 0
    locale.grouping = [5, 3, 1, 0];
    locale.group_repeat = true;
    result = locale.apply_grouping(input);
    assert_eq!(result, "1234567!8!901!23456");

    locale.grouping = [5, 3, 1, 2];
    locale.group_repeat = false;
    result = locale.apply_grouping(input);
    assert_eq!(result, "12345!67!8!901!23456");

    locale.grouping = [5, 3, 1, 2];
    locale.group_repeat = true;
    result = locale.apply_grouping(input);
    assert_eq!(result, "1!23!45!67!8!901!23456");
}

#[test]
#[should_panic]
fn test_thousands_grouping_length_panics_if_no_sep() {
    // We should panic if we try to group with no thousands separator.
    assert_eq!(C_LOCALE.thousands_sep, None);
    C_LOCALE.apply_grouping("123");
}

#[test]
fn test_thousands_grouping_length() {
    fn validate_grouping_length_hint(locale: Locale, mut input: &str) {
        loop {
            let expected = locale.separator_count(input.len()) + input.len();
            let actual = locale.apply_grouping(input).len();
            assert_eq!(expected, actual);
            if input.is_empty() {
                break;
            }
            input = &input[1..];
        }
    }

    validate_grouping_length_hint(EN_US_LOCALE, "123456789");

    // Test weird locales.
    let input = "1234567890123456";
    let mut locale: Locale = C_LOCALE;
    locale.thousands_sep = Some('!');

    locale.grouping = [5, 3, 1, 0];
    locale.group_repeat = false;
    validate_grouping_length_hint(locale, input);

    // group_repeat doesn't matter because trailing group is 0
    locale.grouping = [5, 3, 1, 0];
    locale.group_repeat = true;
    validate_grouping_length_hint(locale, input);

    locale.grouping = [5, 3, 1, 2];
    locale.group_repeat = false;
    validate_grouping_length_hint(locale, input);

    locale.grouping = [5, 3, 1, 2];
    locale.group_repeat = true;
    validate_grouping_length_hint(locale, input);
}
