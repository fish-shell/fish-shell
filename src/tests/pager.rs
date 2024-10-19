use crate::common::get_ellipsis_char;
use crate::complete::{CompleteFlags, Completion};
use crate::pager::{Pager, SelectionMotion};
use crate::termsize::Termsize;
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use crate::wcstringutil::StringFuzzyMatch;

#[test]
#[serial]
fn test_pager_navigation() {
    let _cleanup = test_init();
    // Generate 19 strings of width 10. There's 2 spaces between completions, and our term size is
    // 80; these can therefore fit into 6 columns (6 * 12 - 2 = 70) or 5 columns (58) but not 7
    // columns (7 * 12 - 2 = 82).
    //
    // You can simulate this test by creating 19 files named "file00.txt" through "file_18.txt".
    let mut completions = vec![];
    for _ in 0..19 {
        completions.push(Completion::new(
            L!("abcdefghij").to_owned(),
            "".into(),
            StringFuzzyMatch::exact_match(),
            CompleteFlags::default(),
        ));
    }

    let mut pager = Pager::default();
    pager.set_completions(&completions, true);
    pager.set_term_size(&Termsize::defaults());
    let mut render = pager.render();

    assert_eq!(render.term_width, Some(80));
    assert_eq!(render.term_height, Some(24));

    let rows = 4;
    let cols = 5;

    // We have 19 completions. We can fit into 6 columns with 4 rows or 5 columns with 4 rows; the
    // second one is better and so is what we ought to have picked.
    assert_eq!(render.rows, rows);
    assert_eq!(render.cols, cols);

    // Initially expect to have no completion index.
    assert!(render.selected_completion_idx.is_none());

    // Here are navigation directions and where we expect the selection to be.
    macro_rules! validate {
        ($pager:ident, $render:ident, $dir:expr, $sel:expr) => {
            $pager.select_next_completion_in_direction($dir, &$render);
            $pager.update_rendering(&mut $render);
            assert_eq!(
                Some($sel),
                $render.selected_completion_idx,
                "For command {:?}",
                $dir
            );
        };
    }

    // Tab completion to get into the list.
    validate!(pager, render, SelectionMotion::Next, 0);
    // Westward motion in upper left goes to the last filled column in the last row.
    validate!(pager, render, SelectionMotion::West, 15);
    // East goes back.
    validate!(pager, render, SelectionMotion::East, 0);
    validate!(pager, render, SelectionMotion::West, 15);
    validate!(pager, render, SelectionMotion::West, 11);
    validate!(pager, render, SelectionMotion::East, 15);
    validate!(pager, render, SelectionMotion::East, 0);
    // "Next" motion goes down the column.
    validate!(pager, render, SelectionMotion::Next, 1);
    validate!(pager, render, SelectionMotion::Next, 2);
    validate!(pager, render, SelectionMotion::West, 17);
    validate!(pager, render, SelectionMotion::East, 2);
    validate!(pager, render, SelectionMotion::East, 6);
    validate!(pager, render, SelectionMotion::East, 10);
    validate!(pager, render, SelectionMotion::East, 14);
    validate!(pager, render, SelectionMotion::East, 18);
    validate!(pager, render, SelectionMotion::West, 14);
    validate!(pager, render, SelectionMotion::East, 18);
    // Eastward motion wraps back to the upper left, westward goes to the prior column.
    validate!(pager, render, SelectionMotion::East, 3);
    validate!(pager, render, SelectionMotion::East, 7);
    validate!(pager, render, SelectionMotion::East, 11);
    validate!(pager, render, SelectionMotion::East, 15);
    // Pages.
    validate!(pager, render, SelectionMotion::PageNorth, 12);
    validate!(pager, render, SelectionMotion::PageSouth, 15);
    validate!(pager, render, SelectionMotion::PageNorth, 12);
    validate!(pager, render, SelectionMotion::East, 16);
    validate!(pager, render, SelectionMotion::PageSouth, 18);
    validate!(pager, render, SelectionMotion::East, 3);
    validate!(pager, render, SelectionMotion::North, 2);
    validate!(pager, render, SelectionMotion::PageNorth, 0);
    validate!(pager, render, SelectionMotion::PageSouth, 3);
}

#[test]
#[serial]
fn test_pager_layout() {
    let _cleanup = test_init();
    // These tests are woefully incomplete
    // They only test the truncation logic for a single completion

    let rendered_line = |pager: &mut Pager, width: isize| {
        pager.set_term_size(&Termsize::new(width, 24));
        let rendering = pager.render();
        let sd = &rendering.screen_data;
        assert_eq!(sd.line_count(), 1);
        let line = sd.line(0);
        WString::from(Vec::from_iter((0..line.len()).map(|i| line.char_at(i))))
    };
    let compute_expected = |expected: &wstr| {
        let ellipsis_char = get_ellipsis_char();
        if ellipsis_char != '\u{2026}' {
            // hack: handle the case where ellipsis is not L'\x2026'
            expected.replace(L!("\u{2026}"), wstr::from_char_slice(&[ellipsis_char]))
        } else {
            expected.to_owned()
        }
    };

    macro_rules! validate {
        ($pager:expr, $width:expr, $expected:expr) => {
            assert_eq!(
                rendered_line($pager, $width),
                compute_expected($expected),
                "width {}",
                $width
            );
        };
    }

    let mut pager = Pager::default();

    // These test cases have equal completions and descriptions
    let c1s = vec![Completion::new(
        L!("abcdefghij").to_owned(),
        L!("1234567890").to_owned(),
        StringFuzzyMatch::exact_match(),
        CompleteFlags::default(),
    )];
    pager.set_completions(&c1s, true);

    validate!(&mut pager, 26, L!("abcdefghij  (1234567890)"));
    validate!(&mut pager, 25, L!("abcdefghij  (1234567890)"));
    validate!(&mut pager, 24, L!("abcdefghij  (1234567890)"));
    validate!(&mut pager, 23, L!("abcdefghij  (12345678…)"));
    validate!(&mut pager, 22, L!("abcdefghij  (1234567…)"));
    validate!(&mut pager, 21, L!("abcdefghij  (123456…)"));
    validate!(&mut pager, 20, L!("abcdefghij  (12345…)"));
    validate!(&mut pager, 19, L!("abcdefghij  (1234…)"));
    validate!(&mut pager, 18, L!("abcdefgh…  (1234…)"));
    validate!(&mut pager, 17, L!("abcdefg…  (1234…)"));
    validate!(&mut pager, 16, L!("abcdefg…  (123…)"));

    // These test cases have heavyweight completions
    let c2s = vec![Completion::new(
        L!("abcdefghijklmnopqrs").to_owned(),
        L!("1").to_owned(),
        StringFuzzyMatch::exact_match(),
        CompleteFlags::default(),
    )];
    pager.set_completions(&c2s, true);
    validate!(&mut pager, 26, L!("abcdefghijklmnopqrs  (1)"));
    validate!(&mut pager, 25, L!("abcdefghijklmnopqrs  (1)"));
    validate!(&mut pager, 24, L!("abcdefghijklmnopqrs  (1)"));
    validate!(&mut pager, 23, L!("abcdefghijklmnopq…  (1)"));
    validate!(&mut pager, 22, L!("abcdefghijklmnop…  (1)"));
    validate!(&mut pager, 21, L!("abcdefghijklmno…  (1)"));
    validate!(&mut pager, 20, L!("abcdefghijklmn…  (1)"));
    validate!(&mut pager, 19, L!("abcdefghijklm…  (1)"));
    validate!(&mut pager, 18, L!("abcdefghijkl…  (1)"));
    validate!(&mut pager, 17, L!("abcdefghijk…  (1)"));
    validate!(&mut pager, 16, L!("abcdefghij…  (1)"));

    // These test cases have no descriptions
    let c3s = vec![Completion::new(
        L!("abcdefghijklmnopqrst").to_owned(),
        L!("").to_owned(),
        StringFuzzyMatch::exact_match(),
        CompleteFlags::default(),
    )];
    pager.set_completions(&c3s, true);
    validate!(&mut pager, 26, L!("abcdefghijklmnopqrst"));
    validate!(&mut pager, 25, L!("abcdefghijklmnopqrst"));
    validate!(&mut pager, 24, L!("abcdefghijklmnopqrst"));
    validate!(&mut pager, 23, L!("abcdefghijklmnopqrst"));
    validate!(&mut pager, 22, L!("abcdefghijklmnopqrst"));
    validate!(&mut pager, 21, L!("abcdefghijklmnopqrst"));
    validate!(&mut pager, 20, L!("abcdefghijklmnopqrst"));
    validate!(&mut pager, 19, L!("abcdefghijklmnopqr…"));
    validate!(&mut pager, 18, L!("abcdefghijklmnopq…"));
    validate!(&mut pager, 17, L!("abcdefghijklmnop…"));
    validate!(&mut pager, 16, L!("abcdefghijklmno…"));

    // Newlines in prefix
    let c4s = vec![Completion::new(
        L!("Hello").to_owned(),
        L!("").to_owned(),
        StringFuzzyMatch::exact_match(),
        CompleteFlags::default(),
    )];
    pager.set_prefix(L!("{\\\n"), false); // }
    pager.set_completions(&c4s, true);
    validate!(&mut pager, 30, L!("{\\␊Hello")); // }
}
