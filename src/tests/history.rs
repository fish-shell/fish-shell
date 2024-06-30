use crate::common::{is_windows_subsystem_for_linux, str2wcstring, wcs2osstring, wcs2string, WSL};
use crate::env::{EnvMode, EnvStack};
use crate::history::{self, History, HistoryItem, HistorySearch, PathList, SearchDirection};
use crate::path::path_get_data;
use crate::tests::prelude::*;
use crate::tests::string_escape::ESCAPE_TEST_CHAR;
use crate::wchar::prelude::*;
use crate::wcstringutil::{string_prefixes_string, string_prefixes_string_case_insensitive};
use rand::random;
use std::collections::VecDeque;
use std::ffi::CString;
use std::io::BufReader;
use std::time::SystemTime;
use std::time::UNIX_EPOCH;

fn history_contains(history: &History, txt: &wstr) -> bool {
    for i in 1.. {
        let Some(item) = history.item_at_index(i) else {
            break;
        };

        if item.str() == txt {
            return true;
        }
    }

    false
}

fn random_string() -> WString {
    let mut result = WString::new();
    let max = 1 + random::<usize>() % 32;
    for _ in 0..max {
        let c = char::from_u32(u32::try_from(1 + random::<usize>() % ESCAPE_TEST_CHAR).unwrap())
            .unwrap();
        result.push(c);
    }
    result
}

#[test]
#[serial]
fn test_history() {
    let _cleanup = test_init();
    macro_rules! test_history_matches {
        ($search:expr, $expected:expr) => {
            let expected: Vec<&wstr> = $expected;
            let mut found = vec![];
            while $search.go_to_next_match(SearchDirection::Backward) {
                found.push($search.current_string().to_owned());
            }
            assert_eq!(expected, found);
        };
    }

    let items = [
        L!("Gamma"),
        L!("beta"),
        L!("BetA"),
        L!("Beta"),
        L!("alpha"),
        L!("AlphA"),
        L!("Alpha"),
        L!("alph"),
        L!("ALPH"),
        L!("ZZZ"),
    ];
    let nocase = history::SearchFlags::IGNORE_CASE;

    // Populate a history.
    let history = History::with_name(L!("test_history"));
    history.clear();
    for s in items {
        history.add_commandline(s.to_owned());
    }

    // Helper to set expected items to those matching a predicate, in reverse order.
    let set_expected = |filt: fn(&wstr) -> bool| {
        let mut expected = vec![];
        for s in items {
            if filt(s) {
                expected.push(s);
            }
        }
        expected.reverse();
        expected
    };

    // Items matching "a", case-sensitive.
    let mut searcher = HistorySearch::new(history.clone(), L!("a").to_owned());
    let expected = set_expected(|s| s.contains('a'));
    test_history_matches!(searcher, expected);

    // Items matching "alpha", case-insensitive.
    let mut searcher =
        HistorySearch::new_with_flags(history.clone(), L!("AlPhA").to_owned(), nocase);
    let expected = set_expected(|s| s.to_lowercase().find(L!("alpha")).is_some());
    test_history_matches!(searcher, expected);

    // Items matching "et", case-sensitive.
    let mut searcher = HistorySearch::new(history.clone(), L!("et").to_owned());
    let expected = set_expected(|s| s.find(L!("et")).is_some());
    test_history_matches!(searcher, expected);

    // Items starting with "be", case-sensitive.
    let mut searcher = HistorySearch::new_with_type(
        history.clone(),
        L!("be").to_owned(),
        history::SearchType::Prefix,
    );
    let expected = set_expected(|s| string_prefixes_string(L!("be"), s));
    test_history_matches!(searcher, expected);

    // Items starting with "be", case-insensitive.
    let mut searcher = HistorySearch::new_with(
        history.clone(),
        L!("be").to_owned(),
        history::SearchType::Prefix,
        nocase,
        0,
    );
    let expected = set_expected(|s| string_prefixes_string_case_insensitive(L!("be"), s));
    test_history_matches!(searcher, expected);

    // Items exactly matching "alph", case-sensitive.
    let mut searcher = HistorySearch::new_with_type(
        history.clone(),
        L!("alph").to_owned(),
        history::SearchType::Exact,
    );
    let expected = set_expected(|s| s == "alph");
    test_history_matches!(searcher, expected);

    // Items exactly matching "alph", case-insensitive.
    let mut searcher = HistorySearch::new_with(
        history.clone(),
        L!("alph").to_owned(),
        history::SearchType::Exact,
        nocase,
        0,
    );
    let expected = set_expected(|s| s.to_lowercase() == "alph");
    test_history_matches!(searcher, expected);

    // Test item removal case-sensitive.
    let mut searcher = HistorySearch::new(history.clone(), L!("Alpha").to_owned());
    test_history_matches!(searcher, vec![L!("Alpha")]);
    history.remove(L!("Alpha"));
    let mut searcher = HistorySearch::new(history.clone(), L!("Alpha").to_owned());
    test_history_matches!(searcher, vec![]);

    // Test history escaping and unescaping, yaml, etc.
    let mut before: VecDeque<HistoryItem> = VecDeque::new();
    let mut after: VecDeque<HistoryItem> = VecDeque::new();
    history.clear();
    let max = 100;
    for i in 1..=max {
        // Generate a value.
        let mut value = WString::from_str("test item ") + &i.to_wstring()[..];

        // Maybe add some backslashes.
        if i % 3 == 0 {
            value += L!("(slashies \\\\\\ slashies)");
        }

        // Generate some paths.
        let mut paths = PathList::new();
        for _ in 0..random::<usize>() % 6 {
            paths.push(random_string());
        }

        // Record this item.
        let mut item =
            HistoryItem::new(value, SystemTime::now(), 0, history::PersistenceMode::Disk);
        item.set_required_paths(paths);
        before.push_back(item.clone());
        history.add(item, false);
    }
    history.save();

    // Empty items should just be dropped (#6032).
    history.add_commandline(L!("").into());
    assert!(!history.item_at_index(1).unwrap().is_empty());

    // Read items back in reverse order and ensure they're the same.
    for i in (1..=100).rev() {
        after.push_back(history.item_at_index(i).unwrap());
    }
    assert_eq!(before.len(), after.len());
    for i in 0..before.len() {
        let bef = &before[i];
        let aft = &after[i];
        assert_eq!(bef.str(), aft.str());
        assert_eq!(bef.timestamp(), aft.timestamp());
        assert_eq!(bef.get_required_paths(), aft.get_required_paths());
    }

    // Items should be explicitly added to the history.
    history.add_commandline(L!("test-command").into());
    assert!(history_contains(&history, L!("test-command")));

    // Clean up after our tests.
    history.clear();
}

// Wait until the next second.
fn time_barrier() {
    let start = SystemTime::now();
    loop {
        std::thread::sleep(std::time::Duration::from_millis(1));
        if SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_secs()
            != start.duration_since(UNIX_EPOCH).unwrap().as_secs()
        {
            break;
        }
    }
}

fn generate_history_lines(item_count: usize, idx: usize) -> Vec<WString> {
    let mut result = Vec::with_capacity(item_count);
    for i in 0..item_count {
        result.push(sprintf!("%lu %lu", idx, i));
    }
    result
}

fn test_history_races_pound_on_history(item_count: usize, idx: usize) {
    let _cleanup = test_init();
    // Called in child thread to modify history.
    let hist = History::new(L!("race_test"));
    let hist_lines = generate_history_lines(item_count, idx);
    for line in hist_lines {
        hist.add_commandline(line);
        hist.save();
    }
}

#[test]
#[serial]
fn test_history_races() {
    let _cleanup = test_init();
    // This always fails under WSL
    if is_windows_subsystem_for_linux(WSL::V1) {
        return;
    }

    // This fails too often on Github Actions,
    // leading to a bunch of spurious test failures on unrelated PRs.
    // For now it's better to disable it.
    // TODO: Figure out *why* it does that and fix it.
    if std::env::var_os("CI").is_some() {
        return;
    }

    // Testing history race conditions

    // Test concurrent history writing.
    // How many concurrent writers we have
    const RACE_COUNT: usize = 4;

    // How many items each writer makes
    const ITEM_COUNT: usize = 256;

    // Ensure history is clear.
    History::new(L!("race_test")).clear();

    // history::CHAOS_MODE.store(true);

    let mut children = Vec::with_capacity(RACE_COUNT);
    for i in 0..RACE_COUNT {
        children.push(std::thread::spawn(move || {
            test_history_races_pound_on_history(ITEM_COUNT, i);
        }));
    }

    // Wait for all children.
    for child in children {
        child.join().unwrap();
    }

    // Compute the expected lines.
    let mut expected_lines: [Vec<WString>; RACE_COUNT] =
        std::array::from_fn(|i| generate_history_lines(ITEM_COUNT, i));

    // Ensure we consider the lines that have been outputted as part of our history.
    time_barrier();

    // Ensure that we got sane, sorted results.
    let hist = History::new(L!("race_test"));
    history::CHAOS_MODE.store(false);

    // History is enumerated from most recent to least
    // Every item should be the last item in some array
    let mut hist_idx = 0;
    loop {
        hist_idx += 1;
        let Some(item) = hist.item_at_index(hist_idx) else {
            break;
        };

        let mut found = false;
        for list in &mut expected_lines {
            let Some(position) = list.iter().position(|elem| *elem == item.str()) else {
                continue;
            };

            // Remove everything from this item on
            let removed = list.splice(position.., []);
            for line in removed.into_iter().rev() {
                println!("Item dropped from history: {line}");
            }

            found = true;
            break;
        }
        if !found {
            println!(
                "Line '{}' found in history, but not found in some array",
                item.str()
            );
            for list in &expected_lines {
                if !list.is_empty() {
                    printf!("\tRemaining: %ls\n", list.last().unwrap())
                }
            }
        }
    }

    // +1 to account for history's 1-based offset
    let expected_idx = RACE_COUNT * ITEM_COUNT + 1;
    assert_eq!(hist_idx, expected_idx);

    for list in expected_lines {
        assert_eq!(list, Vec::<WString>::new(), "Lines still left in the array");
    }
    hist.clear();
}

#[test]
#[serial]
fn test_history_merge() {
    let _cleanup = test_init();
    // In a single fish process, only one history is allowed to exist with the given name But it's
    // common to have multiple history instances with the same name active in different processes,
    // e.g. when you have multiple shells open. We try to get that right and merge all their history
    // together. Test that case.
    const COUNT: usize = 3;
    let name = L!("merge_test");
    let hists = [History::new(name), History::new(name), History::new(name)];
    let texts = [L!("History 1"), L!("History 2"), L!("History 3")];
    let alt_texts = [
        L!("History Alt 1"),
        L!("History Alt 2"),
        L!("History Alt 3"),
    ];

    // Make sure history is clear.
    for hist in &hists {
        hist.clear();
    }

    // Make sure we don't add an item in the same second as we created the history.
    time_barrier();

    // Add a different item to each.
    for i in 0..COUNT {
        hists[i].add_commandline(texts[i].to_owned());
    }

    // Save them.
    for hist in &hists {
        hist.save()
    }

    // Make sure each history contains what it ought to, but they have not leaked into each other.
    #[allow(clippy::needless_range_loop)]
    for i in 0..COUNT {
        for j in 0..COUNT {
            let does_contain = history_contains(&hists[i], texts[j]);
            let should_contain = i == j;
            assert_eq!(should_contain, does_contain);
        }
    }

    // Make a new history. It should contain everything. The time_barrier() is so that the timestamp
    // is newer, since we only pick up items whose timestamp is before the birth stamp.
    time_barrier();
    let everything = History::new(name);
    for text in texts {
        assert!(history_contains(&everything, text));
    }

    // Tell all histories to merge. Now everybody should have everything.
    for hist in &hists {
        hist.incorporate_external_changes();
    }

    // Everyone should also have items in the same order (#2312)
    let hist_vals1 = hists[0].get_history();
    for hist in &hists {
        assert_eq!(hist_vals1, hist.get_history());
    }

    // Add some more per-history items.
    for i in 0..COUNT {
        hists[i].add_commandline(alt_texts[i].to_owned());
    }
    // Everybody should have old items, but only one history should have each new item.
    #[allow(clippy::needless_range_loop)]
    for i in 0..COUNT {
        for j in 0..COUNT {
            // Old item.
            assert!(history_contains(&hists[i], texts[j]));

            // New item.
            let does_contain = history_contains(&hists[i], alt_texts[j]);
            let should_contain = i == j;
            assert_eq!(should_contain, does_contain);
        }
    }

    // Make sure incorporate_external_changes doesn't drop items! (#3496)
    let writer = &hists[0];
    let reader = &hists[1];
    let more_texts = [
        L!("Item_#3496_1"),
        L!("Item_#3496_2"),
        L!("Item_#3496_3"),
        L!("Item_#3496_4"),
        L!("Item_#3496_5"),
        L!("Item_#3496_6"),
    ];
    for i in 0..more_texts.len() {
        // time_barrier because merging will ignore items that may be newer
        if i > 0 {
            time_barrier();
        }
        writer.add_commandline(more_texts[i].to_owned());
        writer.incorporate_external_changes();
        reader.incorporate_external_changes();
        for text in more_texts.iter().take(i) {
            assert!(history_contains(reader, text));
        }
    }
    everything.clear();
}

#[test]
#[serial]
fn test_history_path_detection() {
    let _cleanup = test_init();
    // Regression test for #7582.
    let tmpdirbuff = CString::new("/tmp/fish_test_history.XXXXXX").unwrap();
    let tmpdir = unsafe { libc::mkdtemp(tmpdirbuff.into_raw()) };
    let tmpdir = unsafe { CString::from_raw(tmpdir) };
    let mut tmpdir = str2wcstring(tmpdir.to_bytes());
    if !tmpdir.ends_with('/') {
        tmpdir.push('/');
    }

    // Place one valid file in the directory.
    let filename = L!("testfile");
    std::fs::write(wcs2osstring(&(tmpdir.clone() + filename)), []).unwrap();

    let test_vars = EnvStack::new();
    test_vars.set_one(L!("PWD"), EnvMode::GLOBAL, tmpdir.clone());
    test_vars.set_one(L!("HOME"), EnvMode::GLOBAL, tmpdir.clone());

    let history = History::with_name(L!("path_detection"));
    history.clear();
    assert_eq!(history.size(), 0);
    history.clone().add_pending_with_file_detection(
        L!("cmd0 not/a/valid/path"),
        &test_vars,
        history::PersistenceMode::Disk,
    );
    history.clone().add_pending_with_file_detection(
        &(L!("cmd1 ").to_owned() + filename),
        &test_vars,
        history::PersistenceMode::Disk,
    );
    history.clone().add_pending_with_file_detection(
        &(L!("cmd2 ").to_owned() + &tmpdir[..] + L!("/") + filename),
        &test_vars,
        history::PersistenceMode::Disk,
    );
    history.clone().add_pending_with_file_detection(
        &(L!("cmd3  $HOME/").to_owned() + filename),
        &test_vars,
        history::PersistenceMode::Disk,
    );
    history.clone().add_pending_with_file_detection(
        L!("cmd4  $HOME/notafile"),
        &test_vars,
        history::PersistenceMode::Disk,
    );
    history.clone().add_pending_with_file_detection(
        &(L!("cmd5  ~/").to_owned() + filename),
        &test_vars,
        history::PersistenceMode::Disk,
    );
    history.clone().add_pending_with_file_detection(
        L!("cmd6  ~/notafile"),
        &test_vars,
        history::PersistenceMode::Disk,
    );
    history.clone().add_pending_with_file_detection(
        L!("cmd7  ~/*f*"),
        &test_vars,
        history::PersistenceMode::Disk,
    );
    history.clone().add_pending_with_file_detection(
        L!("cmd8  ~/*zzz*"),
        &test_vars,
        history::PersistenceMode::Disk,
    );
    history.resolve_pending();

    const hist_size: usize = 9;
    assert_eq!(history.size(), 9);

    // Expected sets of paths.
    let expected_paths = [
        vec![],                                   // cmd0
        vec![filename.to_owned()],                // cmd1
        vec![tmpdir + L!("/") + filename],        // cmd2
        vec![L!("$HOME/").to_owned() + filename], // cmd3
        vec![],                                   // cmd4
        vec![L!("~/").to_owned() + filename],     // cmd5
        vec![],                                   // cmd6
        vec![],                                   // cmd7 - we do not expand globs
        vec![],                                   // cmd8
    ];

    let maxlap = 128;
    for _lap in 0..maxlap {
        let mut failures = 0;
        for i in 1..=hist_size {
            if history.item_at_index(i).unwrap().get_required_paths()
                != expected_paths[hist_size - i]
            {
                failures += 1;
            }
        }
        if failures == 0 {
            break;
        }
        // The file detection takes a little time since it occurs in the background.
        // Loop until the test passes.
        std::thread::sleep(std::time::Duration::from_millis(2));
    }
    history.clear();
}

fn install_sample_history(name: &wstr) {
    let path = path_get_data().expect("Failed to get data directory");
    std::fs::copy(
        env!("CARGO_MANIFEST_DIR").to_owned()
            + "/tests/"
            + std::str::from_utf8(&wcs2string(name)).unwrap(),
        wcs2osstring(&(path + L!("/") + name + L!("_history"))),
    )
    .unwrap();
}

#[test]
#[serial]
fn test_history_formats() {
    let _cleanup = test_init();
    // Test inferring and reading legacy and bash history formats.
    let name = L!("history_sample_fish_2_0");
    install_sample_history(name);
    let expected: Vec<WString> = vec![
        "echo this has\\\nbackslashes".into(),
        "function foo\necho bar\nend".into(),
        "echo alpha".into(),
    ];
    let test_history_imported = History::with_name(name);
    assert_eq!(test_history_imported.get_history(), expected);
    test_history_imported.clear();

    // Test bash import
    // The results are in the reverse order that they appear in the bash history file.
    // We don't expect whitespace to be elided (#4908: except for leading/trailing whitespace)
    let expected: Vec<WString> = vec![
        "EOF".into(),
        "sleep 123".into(),
        "posix_cmd_sub $(is supported but only splits on newlines)".into(),
        "posix_cmd_sub \"$(is supported)\"".into(),
        "a && echo valid construct".into(),
        "final line".into(),
        "echo supsup".into(),
        "export XVAR='exported'".into(),
        "history --help".into(),
        "echo foo".into(),
    ];
    let test_history_imported_from_bash = History::with_name(L!("bash_import"));
    let file =
        std::fs::File::open(env!("CARGO_MANIFEST_DIR").to_owned() + "/tests/history_sample_bash")
            .unwrap();
    test_history_imported_from_bash.populate_from_bash(BufReader::new(file));
    assert_eq!(test_history_imported_from_bash.get_history(), expected);
    test_history_imported_from_bash.clear();

    let name = L!("history_sample_corrupt1");
    install_sample_history(name);
    // We simply invoke get_string_representation. If we don't die, the test is a success.
    let test_history_imported_from_corrupted = History::with_name(name);
    let expected: Vec<WString> = vec![
        "no_newline_at_end_of_file".into(),
        "corrupt_prefix".into(),
        "this_command_is_ok".into(),
    ];
    assert_eq!(test_history_imported_from_corrupted.get_history(), expected);
    test_history_imported_from_corrupted.clear();
}
