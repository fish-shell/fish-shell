use crate::common::get_ellipsis_char;
use crate::screen::{LayoutCache, PromptCacheEntry, PromptLayout};
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use crate::wcstringutil::join_strings;

#[test]
#[serial]
fn test_complete() {
    let _cleanup = test_init();
    let mut lc = LayoutCache::new();
    assert_eq!(lc.escape_code_length(L!("")), 0);
    assert_eq!(lc.escape_code_length(L!("abcd")), 0);
    assert_eq!(lc.escape_code_length(L!("\x1B[2J")), 4);
    assert_eq!(
        lc.escape_code_length(L!("\x1B[38;5;123mABC")),
        "\x1B[38;5;123m".len()
    );
    assert_eq!(lc.escape_code_length(L!("\x1B@")), 2);

    // iTerm2 escape sequences.
    assert_eq!(
        lc.escape_code_length(L!("\x1B]50;CurrentDir=test/foo\x07NOT_PART_OF_SEQUENCE")),
        25
    );
    assert_eq!(
        lc.escape_code_length(L!("\x1B]50;SetMark\x07NOT_PART_OF_SEQUENCE")),
        13
    );
    assert_eq!(
        lc.escape_code_length(L!("\x1B]6;1;bg;red;brightness;255\x07NOT_PART_OF_SEQUENCE")),
        28
    );
    assert_eq!(
        lc.escape_code_length(L!("\x1B]Pg4040ff\x1B\\NOT_PART_OF_SEQUENCE")),
        12
    );
    assert_eq!(lc.escape_code_length(L!("\x1B]blahblahblah\x1B\\")), 16);
    assert_eq!(lc.escape_code_length(L!("\x1B]blahblahblah\x07")), 15);
}

#[test]
#[serial]
fn test_layout_cache() {
    let _cleanup = test_init();
    let mut seqs = LayoutCache::new();

    // Verify escape code cache.
    assert_eq!(seqs.find_escape_code(L!("abc")), 0);
    seqs.add_escape_code(L!("abc").to_owned());
    seqs.add_escape_code(L!("abc").to_owned());
    assert_eq!(seqs.esc_cache_size(), 1);
    assert_eq!(seqs.find_escape_code(L!("abc")), 3);
    assert_eq!(seqs.find_escape_code(L!("abcd")), 3);
    assert_eq!(seqs.find_escape_code(L!("abcde")), 3);
    assert_eq!(seqs.find_escape_code(L!("xabcde")), 0);
    seqs.add_escape_code(L!("ac").to_owned());
    assert_eq!(seqs.find_escape_code(L!("abcd")), 3);
    assert_eq!(seqs.find_escape_code(L!("acbd")), 2);
    seqs.add_escape_code(L!("wxyz").to_owned());
    assert_eq!(seqs.find_escape_code(L!("abc")), 3);
    assert_eq!(seqs.find_escape_code(L!("abcd")), 3);
    assert_eq!(seqs.find_escape_code(L!("wxyz123")), 4);
    assert_eq!(seqs.find_escape_code(L!("qwxyz123")), 0);
    assert_eq!(seqs.esc_cache_size(), 3);
    seqs.clear();
    assert_eq!(seqs.esc_cache_size(), 0);
    assert_eq!(seqs.find_escape_code(L!("abcd")), 0);

    let huge = usize::MAX;

    // Verify prompt layout cache.
    for i in 0..LayoutCache::PROMPT_CACHE_MAX_SIZE {
        let input = i.to_wstring();
        assert!(!seqs.find_prompt_layout(&input, usize::MAX));
        seqs.add_prompt_layout(PromptCacheEntry {
            text: input.clone(),
            max_line_width: huge,
            trunc_text: input.clone(),
            layout: PromptLayout {
                line_breaks: vec![],
                max_line_width: i,
                last_line_width: 0,
            },
        });
        assert!(seqs.find_prompt_layout(&input, usize::MAX));
        assert_eq!(seqs.prompt_cache.front().unwrap().layout.max_line_width, i);
    }

    let expected_evictee = 3;
    for i in 0..LayoutCache::PROMPT_CACHE_MAX_SIZE {
        if i != expected_evictee {
            assert!(seqs.find_prompt_layout(&i.to_wstring(), usize::MAX));
            assert_eq!(seqs.prompt_cache.front().unwrap().layout.max_line_width, i);
        }
    }

    seqs.add_prompt_layout(PromptCacheEntry {
        text: "whatever".into(),
        max_line_width: huge,
        trunc_text: "whatever".into(),
        layout: PromptLayout {
            line_breaks: vec![],
            max_line_width: 100,
            last_line_width: 0,
        },
    });
    assert!(!seqs.find_prompt_layout(&expected_evictee.to_wstring(), usize::MAX));
    assert!(seqs.find_prompt_layout(L!("whatever"), huge));
    assert_eq!(
        seqs.prompt_cache.front().unwrap().layout.max_line_width,
        100
    );
}

#[test]
#[serial]
fn test_prompt_truncation() {
    let _cleanup = test_init();
    let mut cache = LayoutCache::new();
    let mut trunc = WString::new();

    let ellipsis = || WString::from_chars([get_ellipsis_char()]);

    // No truncation.
    let layout = cache.calc_prompt_layout(L!("abcd"), Some(&mut trunc), usize::MAX);
    assert_eq!(
        layout,
        PromptLayout {
            line_breaks: vec![],
            max_line_width: 4,
            last_line_width: 4,
        }
    );
    assert_eq!(trunc, L!("abcd"));

    // Line break calculation.
    let layout = cache.calc_prompt_layout(
        L!(concat!(
            "0123456789ABCDEF\n",
            "012345\n",
            "0123456789abcdef\n",
            "xyz"
        )),
        Some(&mut trunc),
        80,
    );
    assert_eq!(
        layout,
        PromptLayout {
            line_breaks: vec![16, 23, 40],
            max_line_width: 16,
            last_line_width: 3,
        }
    );

    // Basic truncation.
    let layout = cache.calc_prompt_layout(L!("0123456789ABCDEF"), Some(&mut trunc), 8);
    assert_eq!(
        layout,
        PromptLayout {
            line_breaks: vec![],
            max_line_width: 8,
            last_line_width: 8,
        },
    );
    assert_eq!(trunc, ellipsis() + L!("9ABCDEF"));

    // Multiline truncation.
    let layout = cache.calc_prompt_layout(
        L!(concat!(
            "0123456789ABCDEF\n",
            "012345\n",
            "0123456789abcdef\n",
            "xyz"
        )),
        Some(&mut trunc),
        8,
    );
    assert_eq!(
        layout,
        PromptLayout {
            line_breaks: vec![8, 15, 24],
            max_line_width: 8,
            last_line_width: 3,
        },
    );
    assert_eq!(
        trunc,
        join_strings(
            &[
                ellipsis() + L!("9ABCDEF"),
                L!("012345").to_owned(),
                ellipsis() + L!("9abcdef"),
                L!("xyz").to_owned(),
            ],
            '\n',
        ),
    );

    // Escape sequences are not truncated.
    let layout = cache.calc_prompt_layout(
        L!("\x1B]50;CurrentDir=test/foo\x07NOT_PART_OF_SEQUENCE"),
        Some(&mut trunc),
        4,
    );
    assert_eq!(
        layout,
        PromptLayout {
            line_breaks: vec![],
            max_line_width: 4,
            last_line_width: 4,
        },
    );
    assert_eq!(trunc, ellipsis() + L!("\x1B]50;CurrentDir=test/foo\x07NCE"));

    // Newlines in escape sequences are skipped.
    let layout = cache.calc_prompt_layout(
        L!("\x1B]50;CurrentDir=\ntest/foo\x07NOT_PART_OF_SEQUENCE"),
        Some(&mut trunc),
        4,
    );
    assert_eq!(
        layout,
        PromptLayout {
            line_breaks: vec![],
            max_line_width: 4,
            last_line_width: 4,
        },
    );
    assert_eq!(
        trunc,
        ellipsis() + L!("\x1B]50;CurrentDir=\ntest/foo\x07NCE")
    );

    // We will truncate down to one character if we have to.
    let layout = cache.calc_prompt_layout(L!("Yay"), Some(&mut trunc), 1);
    assert_eq!(
        layout,
        PromptLayout {
            line_breaks: vec![],
            max_line_width: 1,
            last_line_width: 1,
        },
    );
    assert_eq!(trunc, ellipsis());
}
