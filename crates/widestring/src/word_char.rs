//! Support for character classification for vi-mode word movements

use std::{cmp::Ordering, ops::RangeInclusive};

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

pub fn is_blank(c: char) -> bool {
    WordCharClass::from_char(c) == WordCharClass::Blank
}

impl WordCharClass {
    /// Reference: <https://github.com/vim/vim/blob/48940d94/src/mbyte.c#L2866-L2982>
    pub fn from_char(c: char) -> Self {
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
            .binary_search_by(|interval| compare_range_to_char(&interval.range, c))
            .map_or(
                // most other characters are "word" characters
                WordCharClass::Word,
                |i| CLASSES[i].class,
            )
    }
}

/// Check if codepoint is a word character (alphanumeric)
/// Note: Different from vim default behavior, we do not regard underscore as a word character!
fn is_latin1_word_char(c: char) -> bool {
    c.is_ascii_alphanumeric() || (matches!( c, | 'À'..='ÿ') && c != '×' && c != '÷')
}

fn compare_range_to_char(range: &RangeInclusive<char>, c: char) -> Ordering {
    if *range.end() < c {
        Ordering::Less
    } else if *range.start() > c {
        Ordering::Greater
    } else {
        Ordering::Equal
    }
}

/// Check if codepoint is in emoji table using binary search (like vim's intable)
fn is_emoji(c: char) -> bool {
    EMOJI_ALL
        .binary_search_by(|range| compare_range_to_char(range, c))
        .is_ok()
}

/// Character class interval
struct ClassInterval {
    range: RangeInclusive<char>,
    class: WordCharClass,
}

impl ClassInterval {
    const fn new(range: RangeInclusive<char>, class: WordCharClass) -> Self {
        Self { range, class }
    }

    const fn single(c: char, class: WordCharClass) -> Self {
        Self::new(c..=c, class)
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
        I::new('\u{055a}'..='\u{055f}', Punctuation), // Armenian punctuation
        I::single('\u{0589}', Punctuation), // Armenian full stop
        I::single('\u{05be}', Punctuation),
        I::single('\u{05c0}', Punctuation),
        I::single('\u{05c3}', Punctuation),
        I::new('\u{05f3}'..='\u{05f4}', Punctuation),
        I::single('\u{060c}', Punctuation),
        I::single('\u{061b}', Punctuation),
        I::single('\u{061f}', Punctuation),
        I::new('\u{066a}'..='\u{066d}', Punctuation),
        I::single('\u{06d4}', Punctuation),
        I::new('\u{0700}'..='\u{070d}', Punctuation), // Syriac punctuation
        I::new('\u{0964}'..='\u{0965}', Punctuation),
        I::single('\u{0970}', Punctuation),
        I::single('\u{0df4}', Punctuation),
        I::single('\u{0e4f}', Punctuation),
        I::new('\u{0e5a}'..='\u{0e5b}', Punctuation),
        I::new('\u{0f04}'..='\u{0f12}', Punctuation),
        I::new('\u{0f3a}'..='\u{0f3d}', Punctuation),
        I::single('\u{0f85}', Punctuation),
        I::new('\u{104a}'..='\u{104f}', Punctuation), // Myanmar punctuation
        I::single('\u{10fb}', Punctuation),           // Georgian punctuation
        I::new('\u{1361}'..='\u{1368}', Punctuation), // Ethiopic punctuation
        I::new('\u{166d}'..='\u{166e}', Punctuation), // Canadian Syl. punctuation
        I::single('\u{1680}', Blank),
        I::new('\u{169b}'..='\u{169c}', Punctuation),
        I::new('\u{16eb}'..='\u{16ed}', Punctuation),
        I::new('\u{1735}'..='\u{1736}', Punctuation),
        I::new('\u{17d4}'..='\u{17dc}', Punctuation), // Khmer punctuation
        I::new('\u{1800}'..='\u{180a}', Punctuation), // Mongolian punctuation
        I::new('\u{2000}'..='\u{200b}', Blank),       // spaces
        I::new('\u{200c}'..='\u{2027}', Punctuation), // punctuation and symbols
        I::new('\u{2028}'..='\u{2029}', Blank),
        I::new('\u{202a}'..='\u{202e}', Punctuation), // punctuation and symbols
        I::single('\u{202f}', Blank),
        I::new('\u{2030}'..='\u{205e}', Punctuation), // punctuation and symbols
        I::single('\u{205f}', Blank),
        I::new('\u{2060}'..='\u{206f}', Punctuation), // punctuation and symbols
        I::new('\u{2070}'..='\u{207f}', Superscript),
        I::new('\u{2080}'..='\u{2094}', Subscript),
        I::new('\u{20a0}'..='\u{27ff}', Punctuation), // all kinds of symbols
        I::new('\u{2800}'..='\u{28ff}', Braille),
        I::new('\u{2900}'..='\u{2998}', Punctuation), // arrows, brackets, etc.
        I::new('\u{29d8}'..='\u{29db}', Punctuation),
        I::new('\u{29fc}'..='\u{29fd}', Punctuation),
        I::new('\u{2e00}'..='\u{2e7f}', Punctuation), // supplemental punctuation
        I::single('\u{3000}', Blank),                 // ideographic space
        I::new('\u{3001}'..='\u{3020}', Punctuation), // ideographic punctuation
        I::single('\u{3030}', Punctuation),
        I::single('\u{303d}', Punctuation),
        I::new('\u{3040}'..='\u{309f}', Hiragana),
        I::new('\u{30a0}'..='\u{30ff}', Katakana),
        I::new('\u{3300}'..='\u{9fff}', Cjk),
        I::new('\u{ac00}'..='\u{d7a3}', Hangul),
        I::new('\u{f900}'..='\u{faff}', Cjk),
        I::new('\u{fd3e}'..='\u{fd3f}', Punctuation),
        I::new('\u{fe30}'..='\u{fe6b}', Punctuation), // punctuation forms
        I::new('\u{ff00}'..='\u{ff0f}', Punctuation), // half/fullwidth ASCII
        I::new('\u{ff1a}'..='\u{ff20}', Punctuation), // half/fullwidth ASCII
        I::new('\u{ff3b}'..='\u{ff40}', Punctuation), // half/fullwidth ASCII
        I::new('\u{ff5b}'..='\u{ff65}', Punctuation), // half/fullwidth ASCII
        I::new('\u{1d000}'..='\u{1d24f}', Punctuation), // Musical notation
        I::new('\u{1d400}'..='\u{1d7ff}', Punctuation), // Mathematical Alphanumeric Symbols
        I::new('\u{1f000}'..='\u{1f2ff}', Punctuation), // Game pieces; enclosed characters
        I::new('\u{1f300}'..='\u{1f9ff}', Punctuation), // Many symbol blocks
        I::new('\u{20000}'..='\u{2a6df}', Cjk),
        I::new('\u{2a700}'..='\u{2b73f}', Cjk),
        I::new('\u{2b740}'..='\u{2b81f}', Cjk),
        I::new('\u{2f800}'..='\u{2fa1f}', Cjk),
    ]
};

/// Reference: <https://github.com/vim/vim/blob/48940d94/src/mbyte.c#L2704-L2852>
static EMOJI_ALL: &[RangeInclusive<char>] = &[
    '\u{203c}'..='\u{203c}',
    '\u{2049}'..='\u{2049}',
    '\u{2122}'..='\u{2122}',
    '\u{2139}'..='\u{2139}',
    '\u{2194}'..='\u{2199}',
    '\u{21a9}'..='\u{21aa}',
    '\u{231a}'..='\u{231b}',
    '\u{2328}'..='\u{2328}',
    '\u{23cf}'..='\u{23cf}',
    '\u{23e9}'..='\u{23f3}',
    '\u{23f8}'..='\u{23fa}',
    '\u{24c2}'..='\u{24c2}',
    '\u{25aa}'..='\u{25ab}',
    '\u{25b6}'..='\u{25b6}',
    '\u{25c0}'..='\u{25c0}',
    '\u{25fb}'..='\u{25fe}',
    '\u{2600}'..='\u{2604}',
    '\u{260e}'..='\u{260e}',
    '\u{2611}'..='\u{2611}',
    '\u{2614}'..='\u{2615}',
    '\u{2618}'..='\u{2618}',
    '\u{261d}'..='\u{261d}',
    '\u{2620}'..='\u{2620}',
    '\u{2622}'..='\u{2623}',
    '\u{2626}'..='\u{2626}',
    '\u{262a}'..='\u{262a}',
    '\u{262e}'..='\u{262f}',
    '\u{2638}'..='\u{263a}',
    '\u{2640}'..='\u{2640}',
    '\u{2642}'..='\u{2642}',
    '\u{2648}'..='\u{2653}',
    '\u{265f}'..='\u{2660}',
    '\u{2663}'..='\u{2663}',
    '\u{2665}'..='\u{2666}',
    '\u{2668}'..='\u{2668}',
    '\u{267b}'..='\u{267b}',
    '\u{267e}'..='\u{267f}',
    '\u{2692}'..='\u{2697}',
    '\u{2699}'..='\u{2699}',
    '\u{269b}'..='\u{269c}',
    '\u{26a0}'..='\u{26a1}',
    '\u{26a7}'..='\u{26a7}',
    '\u{26aa}'..='\u{26ab}',
    '\u{26b0}'..='\u{26b1}',
    '\u{26bd}'..='\u{26be}',
    '\u{26c4}'..='\u{26c5}',
    '\u{26c8}'..='\u{26c8}',
    '\u{26ce}'..='\u{26cf}',
    '\u{26d1}'..='\u{26d1}',
    '\u{26d3}'..='\u{26d4}',
    '\u{26e9}'..='\u{26ea}',
    '\u{26f0}'..='\u{26f5}',
    '\u{26f7}'..='\u{26fa}',
    '\u{26fd}'..='\u{26fd}',
    '\u{2702}'..='\u{2702}',
    '\u{2705}'..='\u{2705}',
    '\u{2708}'..='\u{270d}',
    '\u{270f}'..='\u{270f}',
    '\u{2712}'..='\u{2712}',
    '\u{2714}'..='\u{2714}',
    '\u{2716}'..='\u{2716}',
    '\u{271d}'..='\u{271d}',
    '\u{2721}'..='\u{2721}',
    '\u{2728}'..='\u{2728}',
    '\u{2733}'..='\u{2734}',
    '\u{2744}'..='\u{2744}',
    '\u{2747}'..='\u{2747}',
    '\u{274c}'..='\u{274c}',
    '\u{274e}'..='\u{274e}',
    '\u{2753}'..='\u{2755}',
    '\u{2757}'..='\u{2757}',
    '\u{2763}'..='\u{2764}',
    '\u{2795}'..='\u{2797}',
    '\u{27a1}'..='\u{27a1}',
    '\u{27b0}'..='\u{27b0}',
    '\u{27bf}'..='\u{27bf}',
    '\u{2934}'..='\u{2935}',
    '\u{2b05}'..='\u{2b07}',
    '\u{2b1b}'..='\u{2b1c}',
    '\u{2b50}'..='\u{2b50}',
    '\u{2b55}'..='\u{2b55}',
    '\u{3030}'..='\u{3030}',
    '\u{303d}'..='\u{303d}',
    '\u{3297}'..='\u{3297}',
    '\u{3299}'..='\u{3299}',
    '\u{1f004}'..='\u{1f004}',
    '\u{1f0cf}'..='\u{1f0cf}',
    '\u{1f170}'..='\u{1f171}',
    '\u{1f17e}'..='\u{1f17f}',
    '\u{1f18e}'..='\u{1f18e}',
    '\u{1f191}'..='\u{1f19a}',
    '\u{1f1e6}'..='\u{1f1ff}',
    '\u{1f201}'..='\u{1f202}',
    '\u{1f21a}'..='\u{1f21a}',
    '\u{1f22f}'..='\u{1f22f}',
    '\u{1f232}'..='\u{1f23a}',
    '\u{1f250}'..='\u{1f251}',
    '\u{1f300}'..='\u{1f321}',
    '\u{1f324}'..='\u{1f393}',
    '\u{1f396}'..='\u{1f397}',
    '\u{1f399}'..='\u{1f39b}',
    '\u{1f39e}'..='\u{1f3f0}',
    '\u{1f3f3}'..='\u{1f3f5}',
    '\u{1f3f7}'..='\u{1f4fd}',
    '\u{1f4ff}'..='\u{1f53d}',
    '\u{1f549}'..='\u{1f54e}',
    '\u{1f550}'..='\u{1f567}',
    '\u{1f56f}'..='\u{1f570}',
    '\u{1f573}'..='\u{1f57a}',
    '\u{1f587}'..='\u{1f587}',
    '\u{1f58a}'..='\u{1f58d}',
    '\u{1f590}'..='\u{1f590}',
    '\u{1f595}'..='\u{1f596}',
    '\u{1f5a4}'..='\u{1f5a5}',
    '\u{1f5a8}'..='\u{1f5a8}',
    '\u{1f5b1}'..='\u{1f5b2}',
    '\u{1f5bc}'..='\u{1f5bc}',
    '\u{1f5c2}'..='\u{1f5c4}',
    '\u{1f5d1}'..='\u{1f5d3}',
    '\u{1f5dc}'..='\u{1f5de}',
    '\u{1f5e1}'..='\u{1f5e1}',
    '\u{1f5e3}'..='\u{1f5e3}',
    '\u{1f5e8}'..='\u{1f5e8}',
    '\u{1f5ef}'..='\u{1f5ef}',
    '\u{1f5f3}'..='\u{1f5f3}',
    '\u{1f5fa}'..='\u{1f64f}',
    '\u{1f680}'..='\u{1f6c5}',
    '\u{1f6cb}'..='\u{1f6d2}',
    '\u{1f6d5}'..='\u{1f6d7}',
    '\u{1f6dc}'..='\u{1f6e5}',
    '\u{1f6e9}'..='\u{1f6e9}',
    '\u{1f6eb}'..='\u{1f6ec}',
    '\u{1f6f0}'..='\u{1f6f0}',
    '\u{1f6f3}'..='\u{1f6fc}',
    '\u{1f7e0}'..='\u{1f7eb}',
    '\u{1f7f0}'..='\u{1f7f0}',
    '\u{1f90c}'..='\u{1f93a}',
    '\u{1f93c}'..='\u{1f945}',
    '\u{1f947}'..='\u{1f9ff}',
    '\u{1fa70}'..='\u{1fa7c}',
    '\u{1fa80}'..='\u{1fa88}',
    '\u{1fa90}'..='\u{1fabd}',
    '\u{1fabf}'..='\u{1fac5}',
    '\u{1face}'..='\u{1fadb}',
    '\u{1fae0}'..='\u{1fae8}',
    '\u{1faf0}'..='\u{1faf8}',
];

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_blank() {
        assert_eq!(WordCharClass::from_char(' '), WordCharClass::Blank); // space
        assert_eq!(WordCharClass::from_char('\t'), WordCharClass::Blank); // tab
        assert_eq!(WordCharClass::from_char('\0'), WordCharClass::Blank); // NUL
        assert_eq!(WordCharClass::from_char('\u{a0}'), WordCharClass::Blank); // non-breaking space
        assert_eq!(WordCharClass::from_char('\u{3000}'), WordCharClass::Blank); // ideographic space
    }

    #[test]
    fn test_word() {
        assert_eq!(WordCharClass::from_char('a'), WordCharClass::Word); // 'a'
        assert_eq!(WordCharClass::from_char('Z'), WordCharClass::Word); // 'Z'
        assert_eq!(WordCharClass::from_char('0'), WordCharClass::Word); // '0'
        assert_eq!(WordCharClass::from_char('e'), WordCharClass::Word); // 'e' with acute
    }

    #[test]
    fn test_punctuation() {
        assert_eq!(WordCharClass::from_char('.'), WordCharClass::Punctuation);
        assert_eq!(WordCharClass::from_char(','), WordCharClass::Punctuation);
        assert_eq!(WordCharClass::from_char(';'), WordCharClass::Punctuation);
        assert_eq!(WordCharClass::from_char('，'), WordCharClass::Punctuation); // ideographic comma
        assert_eq!(WordCharClass::from_char('_'), WordCharClass::Punctuation);
    }

    #[test]
    fn test_cjk() {
        assert_eq!(WordCharClass::from_char('中'), WordCharClass::Cjk); // CJK character
        assert_eq!(WordCharClass::from_char('𠀀'), WordCharClass::Cjk); // CJK Extension B
    }

    #[test]
    fn test_japanese() {
        assert_eq!(WordCharClass::from_char('あ'), WordCharClass::Hiragana);
        assert_eq!(WordCharClass::from_char('ア'), WordCharClass::Katakana);
    }

    #[test]
    fn test_hangul() {
        assert_eq!(WordCharClass::from_char('한'), WordCharClass::Hangul);
    }

    #[test]
    fn test_emoji() {
        assert_eq!(WordCharClass::from_char('😀'), WordCharClass::Emoji); // grinning face
        assert_eq!(WordCharClass::from_char('🚀'), WordCharClass::Emoji); // rocket
        assert_eq!(WordCharClass::from_char('❤'), WordCharClass::Emoji); // red heart
        assert_eq!(WordCharClass::from_char('✅'), WordCharClass::Emoji); // check mark
    }

    #[test]
    fn test_special() {
        assert_eq!(WordCharClass::from_char('⁰'), WordCharClass::Superscript); // superscript zero
        assert_eq!(WordCharClass::from_char('₀'), WordCharClass::Subscript); // subscript zero
        assert_eq!(WordCharClass::from_char('\u{2800}'), WordCharClass::Braille); // braille blank
    }
}
