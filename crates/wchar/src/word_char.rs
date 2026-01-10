//! Support for character classification for vi-mode word movements

use std::cmp::Ordering;

/// Character class for word movements
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WordCharClass {
    Blank,       // whitespace
    Newline,     // newline
    Punctuation, // punctuation and symbols
    Word,        // word character
    Emoji,       // emoji
    Superscript, // superscript (U+2070-U+207F)
    Subscript,   // subscript (U+2080-U+2094)
    Braille,     // braille (U+2800-U+28FF)
    Hiragana,    // Hiragana (U+3040-U+309F)
    Katakana,    // Katakana (U+30A0-U+30FF)
    Cjk,         // CJK Ideographs
    Hangul,      // Hangul Syllables
}

/// Check if codepoint is in emoji table using binary search (like vim's intable)
fn is_emoji(c: char) -> bool {
    let c = u32::from(c);
    EMOJI_ALL
        .binary_search_by(|&(first, last)| {
            if last < c {
                Ordering::Less
            } else if first > c {
                Ordering::Greater
            } else {
                Ordering::Equal
            }
        })
        .is_ok()
}

/// Check if codepoint is a word character (alphanumeric)
/// Note: Different from vim default behavior, we do not regard underscore as a word character!
fn is_latin1_word_char(c: char) -> bool {
    c.is_ascii_alphanumeric() || (matches!( c, | 'À'..='ÿ') && c != '×' && c != '÷')
}

pub fn is_blank(c: char) -> bool {
    WordCharClass::from(c) == WordCharClass::Blank
}

/// Reference: <https://github.com/vim/vim/blob/48940d94/src/mbyte.c#L2866-L2982>
impl From<char> for WordCharClass {
    fn from(c: char) -> Self {
        // Quick check for Latin1 characters
        if u32::from(c) < 0x100 {
            // newline
            if c == '\n' {
                return WordCharClass::Newline;
            }
            // space, tab, NUL, or non-breaking space
            if matches!(c, ' ' | '\t' | '\0' | '\u{a0}' /* NBSP */) {
                return WordCharClass::Blank;
            }
            if is_latin1_word_char(c) {
                return WordCharClass::Word;
            }
            return WordCharClass::Punctuation;
        }

        // emoji check
        if is_emoji(c) {
            return WordCharClass::Emoji;
        }

        // binary search in table
        CLASSES
            .binary_search_by(|interval| {
                let first = char::try_from(interval.first).unwrap();
                let last = char::try_from(interval.last).unwrap();
                if last < c {
                    Ordering::Less
                } else if first > c {
                    Ordering::Greater
                } else {
                    Ordering::Equal
                }
            })
            .map_or(
                // most other characters are "word" characters
                WordCharClass::Word,
                |i| CLASSES[i].class,
            )
    }
}

/// Character class interval
struct ClassInterval {
    first: u32,
    last: u32,
    class: WordCharClass,
}

impl ClassInterval {
    const fn new(first: char, last: char, class: WordCharClass) -> Self {
        Self {
            first: first as u32,
            last: last as u32,
            class,
        }
    }

    const fn single(c: char, class: WordCharClass) -> Self {
        Self::new(c, c, class)
    }
}

/// Character classification table (sorted non-overlapping intervals)
/// Reference: <https://github.com/vim/vim/blob/48940d94/src/mbyte.c#L2867-L2948>
static CLASSES: &[ClassInterval] = {
    use ClassInterval as I;
    use WordCharClass::*;
    &[
        I::single('\u{037e}', Punctuation), // Greek question mark
        I::single('\u{0387}', Punctuation), // Greek ano teleia
        I::new('\u{055a}', '\u{055f}', Punctuation), // Armenian punctuation
        I::single('\u{0589}', Punctuation), // Armenian full stop
        I::single('\u{05be}', Punctuation),
        I::single('\u{05c0}', Punctuation),
        I::single('\u{05c3}', Punctuation),
        I::new('\u{05f3}', '\u{05f4}', Punctuation),
        I::single('\u{060c}', Punctuation),
        I::single('\u{061b}', Punctuation),
        I::single('\u{061f}', Punctuation),
        I::new('\u{066a}', '\u{066d}', Punctuation),
        I::single('\u{06d4}', Punctuation),
        I::new('\u{0700}', '\u{070d}', Punctuation), // Syriac punctuation
        I::new('\u{0964}', '\u{0965}', Punctuation),
        I::single('\u{0970}', Punctuation),
        I::single('\u{0df4}', Punctuation),
        I::single('\u{0e4f}', Punctuation),
        I::new('\u{0e5a}', '\u{0e5b}', Punctuation),
        I::new('\u{0f04}', '\u{0f12}', Punctuation),
        I::new('\u{0f3a}', '\u{0f3d}', Punctuation),
        I::single('\u{0f85}', Punctuation),
        I::new('\u{104a}', '\u{104f}', Punctuation), // Myanmar punctuation
        I::single('\u{10fb}', Punctuation),          // Georgian punctuation
        I::new('\u{1361}', '\u{1368}', Punctuation), // Ethiopic punctuation
        I::new('\u{166d}', '\u{166e}', Punctuation), // Canadian Syl. punctuation
        I::single('\u{1680}', Blank),
        I::new('\u{169b}', '\u{169c}', Punctuation),
        I::new('\u{16eb}', '\u{16ed}', Punctuation),
        I::new('\u{1735}', '\u{1736}', Punctuation),
        I::new('\u{17d4}', '\u{17dc}', Punctuation), // Khmer punctuation
        I::new('\u{1800}', '\u{180a}', Punctuation), // Mongolian punctuation
        I::new('\u{2000}', '\u{200b}', Blank),       // spaces
        I::new('\u{200c}', '\u{2027}', Punctuation), // punctuation and symbols
        I::new('\u{2028}', '\u{2029}', Blank),
        I::new('\u{202a}', '\u{202e}', Punctuation), // punctuation and symbols
        I::single('\u{202f}', Blank),
        I::new('\u{2030}', '\u{205e}', Punctuation), // punctuation and symbols
        I::single('\u{205f}', Blank),
        I::new('\u{2060}', '\u{206f}', Punctuation), // punctuation and symbols
        I::new('\u{2070}', '\u{207f}', Superscript),
        I::new('\u{2080}', '\u{2094}', Subscript),
        I::new('\u{20a0}', '\u{27ff}', Punctuation), // all kinds of symbols
        I::new('\u{2800}', '\u{28ff}', Braille),
        I::new('\u{2900}', '\u{2998}', Punctuation), // arrows, brackets, etc.
        I::new('\u{29d8}', '\u{29db}', Punctuation),
        I::new('\u{29fc}', '\u{29fd}', Punctuation),
        I::new('\u{2e00}', '\u{2e7f}', Punctuation), // supplemental punctuation
        I::single('\u{3000}', Blank),                // ideographic space
        I::new('\u{3001}', '\u{3020}', Punctuation), // ideographic punctuation
        I::single('\u{3030}', Punctuation),
        I::single('\u{303d}', Punctuation),
        I::new('\u{3040}', '\u{309f}', Hiragana),
        I::new('\u{30a0}', '\u{30ff}', Katakana),
        I::new('\u{3300}', '\u{9fff}', Cjk),
        I::new('\u{ac00}', '\u{d7a3}', Hangul),
        I::new('\u{f900}', '\u{faff}', Cjk),
        I::new('\u{fd3e}', '\u{fd3f}', Punctuation),
        I::new('\u{fe30}', '\u{fe6b}', Punctuation), // punctuation forms
        I::new('\u{ff00}', '\u{ff0f}', Punctuation), // half/fullwidth ASCII
        I::new('\u{ff1a}', '\u{ff20}', Punctuation), // half/fullwidth ASCII
        I::new('\u{ff3b}', '\u{ff40}', Punctuation), // half/fullwidth ASCII
        I::new('\u{ff5b}', '\u{ff65}', Punctuation), // half/fullwidth ASCII
        I::new('\u{1d000}', '\u{1d24f}', Punctuation), // Musical notation
        I::new('\u{1d400}', '\u{1d7ff}', Punctuation), // Mathematical Alphanumeric Symbols
        I::new('\u{1f000}', '\u{1f2ff}', Punctuation), // Game pieces; enclosed characters
        I::new('\u{1f300}', '\u{1f9ff}', Punctuation), // Many symbol blocks
        I::new('\u{20000}', '\u{2a6df}', Cjk),
        I::new('\u{2a700}', '\u{2b73f}', Cjk),
        I::new('\u{2b740}', '\u{2b81f}', Cjk),
        I::new('\u{2f800}', '\u{2fa1f}', Cjk),
    ]
};

/// Reference: <https://github.com/vim/vim/blob/48940d94/src/mbyte.c#L2704-L2852>
static EMOJI_ALL: &[(u32, u32)] = &[
    (0x203c, 0x203c),
    (0x2049, 0x2049),
    (0x2122, 0x2122),
    (0x2139, 0x2139),
    (0x2194, 0x2199),
    (0x21a9, 0x21aa),
    (0x231a, 0x231b),
    (0x2328, 0x2328),
    (0x23cf, 0x23cf),
    (0x23e9, 0x23f3),
    (0x23f8, 0x23fa),
    (0x24c2, 0x24c2),
    (0x25aa, 0x25ab),
    (0x25b6, 0x25b6),
    (0x25c0, 0x25c0),
    (0x25fb, 0x25fe),
    (0x2600, 0x2604),
    (0x260e, 0x260e),
    (0x2611, 0x2611),
    (0x2614, 0x2615),
    (0x2618, 0x2618),
    (0x261d, 0x261d),
    (0x2620, 0x2620),
    (0x2622, 0x2623),
    (0x2626, 0x2626),
    (0x262a, 0x262a),
    (0x262e, 0x262f),
    (0x2638, 0x263a),
    (0x2640, 0x2640),
    (0x2642, 0x2642),
    (0x2648, 0x2653),
    (0x265f, 0x2660),
    (0x2663, 0x2663),
    (0x2665, 0x2666),
    (0x2668, 0x2668),
    (0x267b, 0x267b),
    (0x267e, 0x267f),
    (0x2692, 0x2697),
    (0x2699, 0x2699),
    (0x269b, 0x269c),
    (0x26a0, 0x26a1),
    (0x26a7, 0x26a7),
    (0x26aa, 0x26ab),
    (0x26b0, 0x26b1),
    (0x26bd, 0x26be),
    (0x26c4, 0x26c5),
    (0x26c8, 0x26c8),
    (0x26ce, 0x26cf),
    (0x26d1, 0x26d1),
    (0x26d3, 0x26d4),
    (0x26e9, 0x26ea),
    (0x26f0, 0x26f5),
    (0x26f7, 0x26fa),
    (0x26fd, 0x26fd),
    (0x2702, 0x2702),
    (0x2705, 0x2705),
    (0x2708, 0x270d),
    (0x270f, 0x270f),
    (0x2712, 0x2712),
    (0x2714, 0x2714),
    (0x2716, 0x2716),
    (0x271d, 0x271d),
    (0x2721, 0x2721),
    (0x2728, 0x2728),
    (0x2733, 0x2734),
    (0x2744, 0x2744),
    (0x2747, 0x2747),
    (0x274c, 0x274c),
    (0x274e, 0x274e),
    (0x2753, 0x2755),
    (0x2757, 0x2757),
    (0x2763, 0x2764),
    (0x2795, 0x2797),
    (0x27a1, 0x27a1),
    (0x27b0, 0x27b0),
    (0x27bf, 0x27bf),
    (0x2934, 0x2935),
    (0x2b05, 0x2b07),
    (0x2b1b, 0x2b1c),
    (0x2b50, 0x2b50),
    (0x2b55, 0x2b55),
    (0x3030, 0x3030),
    (0x303d, 0x303d),
    (0x3297, 0x3297),
    (0x3299, 0x3299),
    (0x1f004, 0x1f004),
    (0x1f0cf, 0x1f0cf),
    (0x1f170, 0x1f171),
    (0x1f17e, 0x1f17f),
    (0x1f18e, 0x1f18e),
    (0x1f191, 0x1f19a),
    (0x1f1e6, 0x1f1ff),
    (0x1f201, 0x1f202),
    (0x1f21a, 0x1f21a),
    (0x1f22f, 0x1f22f),
    (0x1f232, 0x1f23a),
    (0x1f250, 0x1f251),
    (0x1f300, 0x1f321),
    (0x1f324, 0x1f393),
    (0x1f396, 0x1f397),
    (0x1f399, 0x1f39b),
    (0x1f39e, 0x1f3f0),
    (0x1f3f3, 0x1f3f5),
    (0x1f3f7, 0x1f4fd),
    (0x1f4ff, 0x1f53d),
    (0x1f549, 0x1f54e),
    (0x1f550, 0x1f567),
    (0x1f56f, 0x1f570),
    (0x1f573, 0x1f57a),
    (0x1f587, 0x1f587),
    (0x1f58a, 0x1f58d),
    (0x1f590, 0x1f590),
    (0x1f595, 0x1f596),
    (0x1f5a4, 0x1f5a5),
    (0x1f5a8, 0x1f5a8),
    (0x1f5b1, 0x1f5b2),
    (0x1f5bc, 0x1f5bc),
    (0x1f5c2, 0x1f5c4),
    (0x1f5d1, 0x1f5d3),
    (0x1f5dc, 0x1f5de),
    (0x1f5e1, 0x1f5e1),
    (0x1f5e3, 0x1f5e3),
    (0x1f5e8, 0x1f5e8),
    (0x1f5ef, 0x1f5ef),
    (0x1f5f3, 0x1f5f3),
    (0x1f5fa, 0x1f64f),
    (0x1f680, 0x1f6c5),
    (0x1f6cb, 0x1f6d2),
    (0x1f6d5, 0x1f6d7),
    (0x1f6dc, 0x1f6e5),
    (0x1f6e9, 0x1f6e9),
    (0x1f6eb, 0x1f6ec),
    (0x1f6f0, 0x1f6f0),
    (0x1f6f3, 0x1f6fc),
    (0x1f7e0, 0x1f7eb),
    (0x1f7f0, 0x1f7f0),
    (0x1f90c, 0x1f93a),
    (0x1f93c, 0x1f945),
    (0x1f947, 0x1f9ff),
    (0x1fa70, 0x1fa7c),
    (0x1fa80, 0x1fa88),
    (0x1fa90, 0x1fabd),
    (0x1fabf, 0x1fac5),
    (0x1face, 0x1fadb),
    (0x1fae0, 0x1fae8),
    (0x1faf0, 0x1faf8),
];

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_blank() {
        assert_eq!(WordCharClass::from(' '), WordCharClass::Blank); // space
        assert_eq!(WordCharClass::from('\t'), WordCharClass::Blank); // tab
        assert_eq!(WordCharClass::from('\0'), WordCharClass::Blank); // NUL
        assert_eq!(WordCharClass::from('\u{a0}'), WordCharClass::Blank); // non-breaking space
        assert_eq!(WordCharClass::from('\u{3000}'), WordCharClass::Blank); // ideographic space
    }

    #[test]
    fn test_word() {
        assert_eq!(WordCharClass::from('a'), WordCharClass::Word); // 'a'
        assert_eq!(WordCharClass::from('Z'), WordCharClass::Word); // 'Z'
        assert_eq!(WordCharClass::from('0'), WordCharClass::Word); // '0'
        assert_eq!(WordCharClass::from('e'), WordCharClass::Word); // 'e' with acute
    }

    #[test]
    fn test_punctuation() {
        assert_eq!(WordCharClass::from('.'), WordCharClass::Punctuation);
        assert_eq!(WordCharClass::from(','), WordCharClass::Punctuation);
        assert_eq!(WordCharClass::from(';'), WordCharClass::Punctuation);
        assert_eq!(WordCharClass::from('，'), WordCharClass::Punctuation); // ideographic comma
        assert_eq!(WordCharClass::from('_'), WordCharClass::Punctuation);
    }

    #[test]
    fn test_cjk() {
        assert_eq!(WordCharClass::from('中'), WordCharClass::Cjk); // CJK character
        assert_eq!(WordCharClass::from('𠀀'), WordCharClass::Cjk); // CJK Extension B
    }

    #[test]
    fn test_japanese() {
        assert_eq!(WordCharClass::from('あ'), WordCharClass::Hiragana);
        assert_eq!(WordCharClass::from('ア'), WordCharClass::Katakana);
    }

    #[test]
    fn test_hangul() {
        assert_eq!(WordCharClass::from('한'), WordCharClass::Hangul);
    }

    #[test]
    fn test_emoji() {
        assert_eq!(WordCharClass::from('😀'), WordCharClass::Emoji); // grinning face
        assert_eq!(WordCharClass::from('🚀'), WordCharClass::Emoji); // rocket
        assert_eq!(WordCharClass::from('❤'), WordCharClass::Emoji); // red heart
        assert_eq!(WordCharClass::from('✅'), WordCharClass::Emoji); // check mark
    }

    #[test]
    fn test_special() {
        assert_eq!(WordCharClass::from('⁰'), WordCharClass::Superscript); // superscript zero
        assert_eq!(WordCharClass::from('₀'), WordCharClass::Subscript); // subscript zero
        assert_eq!(WordCharClass::from('\u{2800}'), WordCharClass::Braille); // braille blank
    }
}
