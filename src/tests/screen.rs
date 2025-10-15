use crate::common::get_ellipsis_char;
use crate::highlight::HighlightSpec;
use crate::parse_util::parse_util_compute_indents;
use crate::screen::{LayoutCache, PromptCacheEntry, PromptLayout, ScreenLayout, compute_layout};
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
                line_starts: vec![],
                last_line_width: i,
            },
        });
        assert!(seqs.find_prompt_layout(&input, usize::MAX));
        assert_eq!(seqs.prompt_cache.front().unwrap().layout.last_line_width, i);
    }

    let expected_evictee = 3;
    for i in 0..LayoutCache::PROMPT_CACHE_MAX_SIZE {
        if i != expected_evictee {
            assert!(seqs.find_prompt_layout(&i.to_wstring(), usize::MAX));
            assert_eq!(seqs.prompt_cache.front().unwrap().layout.last_line_width, i);
        }
    }

    seqs.add_prompt_layout(PromptCacheEntry {
        text: "whatever".into(),
        max_line_width: huge,
        trunc_text: "whatever".into(),
        layout: PromptLayout {
            line_starts: vec![],
            last_line_width: 100,
        },
    });
    assert!(!seqs.find_prompt_layout(&expected_evictee.to_wstring(), usize::MAX));
    assert!(seqs.find_prompt_layout(L!("whatever"), huge));
    assert_eq!(
        seqs.prompt_cache.front().unwrap().layout.last_line_width,
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
            line_starts: vec![0],
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
            line_starts: vec![0, 17, 24, 41],
            last_line_width: 3,
        }
    );

    // Basic truncation.
    let layout = cache.calc_prompt_layout(L!("0123456789ABCDEF"), Some(&mut trunc), 8);
    assert_eq!(
        layout,
        PromptLayout {
            line_starts: vec![0],
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
            line_starts: vec![0, 9, 16, 25],
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
            line_starts: vec![0],
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
            line_starts: vec![0],
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
            line_starts: vec![0],
            last_line_width: 1,
        },
    );
    assert_eq!(trunc, ellipsis());
}

#[test]
fn test_compute_layout() {
    macro_rules! validate {
        (
            (
                $screen_width:expr,
                $left_untrunc_prompt:literal,
                $right_untrunc_prompt:literal,
                $commandline_before_suggestion:literal,
                $autosuggestion_str:literal,
                $commandline_after_suggestion:literal
            )
            -> (
                $left_prompt:literal,
                $left_prompt_space:expr,
                $right_prompt:literal,
                $autosuggestion:literal $(,)?
            )
        ) => {{
            let full_commandline = L!($commandline_before_suggestion).to_owned()
                + L!($autosuggestion_str)
                + L!($commandline_after_suggestion);
            let mut colors = vec![HighlightSpec::default(); full_commandline.len()];
            let mut indent = parse_util_compute_indents(&full_commandline);
            assert_eq!(
                compute_layout(
                    '…',
                    $screen_width,
                    L!($left_untrunc_prompt),
                    L!($right_untrunc_prompt),
                    L!($commandline_before_suggestion),
                    &mut colors,
                    &mut indent,
                    L!($autosuggestion_str),
                ),
                ScreenLayout {
                    left_prompt: L!($left_prompt).to_owned(),
                    left_prompt_space: $left_prompt_space,
                    left_prompt_lines: 1,
                    right_prompt: L!($right_prompt).to_owned(),
                    autosuggestion: L!($autosuggestion).to_owned(),
                }
            );
            indent
        }};
    }

    validate!(
        (
            80, "left>", "<right", "command", " autosuggestion", ""
        ) -> (
            "left>",
            5,
            "<right",
            " autosuggestion",
        )
    );
    validate!(
        (
            30, "left>", "<right", "command", " autosuggesTION", ""
        ) -> (
            "left>",
            5,
            "<right",
            " autosugges…",
        )
    );
    validate!(
        (
            30, "left>", "<right", "foo\ncommand", " autosuggestion", ""
        ) -> (
            "left>",
            5,
            "<right",
            " autosuggestion",
        )
    );
    validate!(
        (
            30, "left>", "<right", "foo\ncommand", " autosuggestion tRUNCATED", ""
        ) -> (
            "left>",
            5,
            "<right",
            " autosuggestion t…",
        )
    );
    validate!(
        (
            30, "left>", "<right", "if :\ncommand", " autosuggestiON  TRUNCATED", ""
        ) -> (
            "left>",
            5,
            "<right",
            " autosuggesti…",
        )
    );
    let indent = validate!(
        (
            30, "left>", "<right", "if :\ncommand", " autosuggestiON  TRUNCATED", "\nfoo"
        ) -> (
            "left>",
            5,
            "<right",
            " autosuggesti…",
        )
    );
    assert_eq!(indent["if :\ncommand autosuggesti…\n".len()], 1);

    validate!(
        (
            18, "left>", "<RIGHT", "command", " autoSUGGESTION", ""
        ) -> (
            "left>",
            5,
            "",
            " auto…",
        )
    );
    validate!(
        (
            18, "left>", "<RIGHT", "command auto", "s", ""
        ) -> (
            "left>",
            5,
            "",
            "s",
        )
    );
    validate!(
        (
            18, "left>", "<RIGHT", "command auto", "SUGGESTION", ""
        ) -> (
            "left>",
            5,
            "",
            "…",
        )
    );
    validate!(
        (
            18, "left>", "<RIGHT", "command autos", "uggestion long soFT WRAP", ""
        ) -> (
            "left>",
            5,
            "",
            "uggestion long so…",
        )
    );
    validate!(
        (
            18, "left>", "<right", "if :\ncomm", "and AUTOSUGGESTION", ""
        ) -> (
            "left>",
            5,
            "<right",
            "and …",
        )
    );
    validate!(
        (
            18, "left>", "<right", "if :\ncommand ", "AUTOSUGGESTION", ""
        ) -> (
            "left>",
            5,
            "<right",
            "…",
        )
    );
    validate!( //
        (
            18, "left>", "<right", "if :\ncommand a", "utosuggestion sofT WRAP", ""
        ) -> (
            "left>",
            5,
            "<right",
            "utosuggestion sof…",
        )
    );
    validate!(
        (
            18, "left>", "<RIGHT", "if true\ncomm", "and AUTOSUGGESTION", "\nfoo"
        ) -> (
            "left>",
            5,
            "",
            "and …",
        )
    );
    validate!(
        (
            18, "left>", "<RIGHT", "if true\ncommand ", "AUTOSUGGESTION", "\nfoo"
        ) -> (
            "left>",
            5,
            "",
            "…",
        )
    );
    validate!(
        (
            18, "left>", "<RIGHT", "if true\ncommand a", "utosuggestion sofT WRAP", "\nfoo"
        ) -> (
            "left>",
            5,
            "",
            "utosuggestion sof…",
        )
    );
}
