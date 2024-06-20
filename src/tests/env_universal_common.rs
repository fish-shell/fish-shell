use crate::common::char_offset;
use crate::common::wcs2osstring;
use crate::common::ScopeGuard;
use crate::common::ENCODE_DIRECT_BASE;
use crate::env::{EnvVar, EnvVarFlags, VarTable};
use crate::env_universal_common::{CallbackDataList, EnvUniversal, UvarFormat};
use crate::reader::{reader_pop, reader_push, ReaderConfig};
use crate::tests::prelude::*;
use crate::threads::{iothread_drain_all, iothread_perform};
use crate::wchar::prelude::*;
use crate::wutil::file_id_for_path;
use crate::wutil::INVALID_FILE_ID;

const UVARS_PER_THREAD: usize = 8;
const UVARS_TEST_PATH: &wstr = L!("test/fish_uvars_test/varsfile.txt");

fn test_universal_helper(x: usize) {
    let _cleanup = test_init();
    let mut callbacks = CallbackDataList::new();
    let mut uvars = EnvUniversal::new();
    uvars.initialize_at_path(&mut callbacks, UVARS_TEST_PATH.to_owned());

    for j in 0..UVARS_PER_THREAD {
        let key = sprintf!("key_%d_%d", x, j);
        let val = sprintf!("val_%d_%d", x, j);
        uvars.set(&key, EnvVar::new(val, EnvVarFlags::empty()));
        let synced = uvars.sync(&mut callbacks);
        assert!(
            synced,
            "Failed to sync universal variables after modification"
        );
    }

    // Last step is to delete the first key.
    uvars.remove(&sprintf!("key_%d_%d", x, 0));
    let synced = uvars.sync(&mut callbacks);
    assert!(synced, "Failed to sync universal variables after deletion");
}

#[test]
#[serial]
fn test_universal() {
    let _cleanup = test_init();
    let _ = std::fs::remove_dir_all("test/fish_uvars_test/");
    std::fs::create_dir_all("test/fish_uvars_test/").unwrap();
    let parser = TestParser::new();

    let mut reader = reader_push(&parser, L!(""), ReaderConfig::default());
    let _pop = ScopeGuard::new((), |()| reader_pop());

    let threads = 1;
    for i in 0..threads {
        iothread_perform(move || test_universal_helper(i));
    }
    iothread_drain_all(&mut reader);

    let mut uvars = EnvUniversal::new();
    let mut callbacks = CallbackDataList::new();
    uvars.initialize_at_path(&mut callbacks, UVARS_TEST_PATH.to_owned());

    for i in 0..threads {
        for j in 0..UVARS_PER_THREAD {
            let key = sprintf!("key_%d_%d", i, j);
            let expected_val = if j == 0 {
                None
            } else {
                Some(EnvVar::new(
                    sprintf!("val_%d_%d", i, j),
                    EnvVarFlags::empty(),
                ))
            };
            let var = uvars.get(&key);
            assert_eq!(var, expected_val);
        }
    }

    std::fs::remove_dir_all("test/fish_uvars_test/").unwrap();
}

#[test]
#[serial]
fn test_universal_output() {
    let _cleanup = test_init();
    let flag_export = EnvVarFlags::EXPORT;
    let flag_pathvar = EnvVarFlags::PATHVAR;

    let mut vars = VarTable::new();
    vars.insert(
        L!("varA").to_owned(),
        EnvVar::new_vec(
            vec![L!("ValA1").to_owned(), L!("ValA2").to_owned()],
            EnvVarFlags::empty(),
        ),
    );
    vars.insert(
        L!("varB").to_owned(),
        EnvVar::new_vec(vec![L!("ValB1").to_owned()], flag_export),
    );
    vars.insert(
        L!("varC").to_owned(),
        EnvVar::new_vec(vec![L!("ValC1").to_owned()], EnvVarFlags::empty()),
    );
    vars.insert(
        L!("varD").to_owned(),
        EnvVar::new_vec(vec![L!("ValD1").to_owned()], flag_export | flag_pathvar),
    );
    vars.insert(
        L!("varE").to_owned(),
        EnvVar::new_vec(
            vec![L!("ValE1").to_owned(), L!("ValE2").to_owned()],
            flag_pathvar,
        ),
    );
    vars.insert(
        L!("varF").to_owned(),
        EnvVar::new_vec(
            vec![WString::from_chars([char_offset(ENCODE_DIRECT_BASE, 0xfc)])],
            EnvVarFlags::empty(),
        ),
    );

    let text = EnvUniversal::serialize_with_vars(&vars);
    let expected = concat!(
        "# This file contains fish universal variable definitions.\n",
        "# VERSION: 3.0\n",
        "SETUVAR varA:ValA1\\x1eValA2\n",
        "SETUVAR --export varB:ValB1\n",
        "SETUVAR varC:ValC1\n",
        "SETUVAR --export --path varD:ValD1\n",
        "SETUVAR --path varE:ValE1\\x1eValE2\n",
        "SETUVAR varF:\\xfc\n",
    )
    .as_bytes();
    assert_eq!(text, expected);
}

#[test]
#[serial]
fn test_universal_parsing() {
    let _cleanup = test_init();
    let input = concat!(
        "# This file contains fish universal variable definitions.\n",
        "# VERSION: 3.0\n",
        "SETUVAR varA:ValA1\\x1eValA2\n",
        "SETUVAR --export varB:ValB1\n",
        "SETUVAR --nonsenseflag varC:ValC1\n",
        "SETUVAR --export --path varD:ValD1\n",
        "SETUVAR --path --path varE:ValE1\\x1eValE2\n",
    )
    .as_bytes();

    let flag_export = EnvVarFlags::EXPORT;
    let flag_pathvar = EnvVarFlags::PATHVAR;

    let mut vars = VarTable::new();

    vars.insert(
        L!("varA").to_owned(),
        EnvVar::new_vec(
            vec![L!("ValA1").to_owned(), L!("ValA2").to_owned()],
            EnvVarFlags::empty(),
        ),
    );
    vars.insert(
        L!("varB").to_owned(),
        EnvVar::new_vec(vec![L!("ValB1").to_owned()], flag_export),
    );
    vars.insert(
        L!("varC").to_owned(),
        EnvVar::new_vec(vec![L!("ValC1").to_owned()], EnvVarFlags::empty()),
    );
    vars.insert(
        L!("varD").to_owned(),
        EnvVar::new_vec(vec![L!("ValD1").to_owned()], flag_export | flag_pathvar),
    );
    vars.insert(
        L!("varE").to_owned(),
        EnvVar::new_vec(
            vec![L!("ValE1").to_owned(), L!("ValE2").to_owned()],
            flag_pathvar,
        ),
    );

    let mut parsed_vars = VarTable::new();
    EnvUniversal::populate_variables(input, &mut parsed_vars);
    assert_eq!(vars, parsed_vars);
}

#[test]
#[serial]
fn test_universal_parsing_legacy() {
    let _cleanup = test_init();
    let input = concat!(
        "# This file contains fish universal variable definitions.\n",
        "SET varA:ValA1\\x1eValA2\n",
        "SET_EXPORT varB:ValB1\n",
    )
    .as_bytes();

    let mut vars = VarTable::new();
    vars.insert(
        L!("varA").to_owned(),
        EnvVar::new_vec(
            vec![L!("ValA1").to_owned(), L!("ValA2").to_owned()],
            EnvVarFlags::empty(),
        ),
    );
    vars.insert(
        L!("varB").to_owned(),
        EnvVar::new(L!("ValB1").to_owned(), EnvVarFlags::EXPORT),
    );

    let mut parsed_vars = VarTable::new();
    EnvUniversal::populate_variables(input, &mut parsed_vars);
    assert_eq!(vars, parsed_vars);
}

#[test]
#[serial]
fn test_universal_callbacks() {
    let _cleanup = test_init();
    std::fs::create_dir_all("test/fish_uvars_test/").unwrap();
    let mut callbacks = CallbackDataList::new();
    let mut uvars1 = EnvUniversal::new();
    let mut uvars2 = EnvUniversal::new();
    uvars1.initialize_at_path(&mut callbacks, UVARS_TEST_PATH.to_owned());
    uvars2.initialize_at_path(&mut callbacks, UVARS_TEST_PATH.to_owned());

    let noflags = EnvVarFlags::empty();

    // Put some variables into both.
    uvars1.set(L!("alpha"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("beta"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("delta"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("epsilon"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("lambda"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("kappa"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("omicron"), EnvVar::new(L!("1").to_owned(), noflags)); //

    uvars1.sync(&mut callbacks);
    uvars2.sync(&mut callbacks);

    // Change uvars1.
    uvars1.set(L!("alpha"), EnvVar::new(L!("2").to_owned(), noflags)); // changes value
    uvars1.set(
        L!("beta"),
        EnvVar::new(L!("1").to_owned(), EnvVarFlags::EXPORT),
    ); // changes export
    uvars1.remove(L!("delta")); // erases value
    uvars1.set(L!("epsilon"), EnvVar::new(L!("1").to_owned(), noflags)); // changes nothing
    uvars1.sync(&mut callbacks);

    // Change uvars2. It should treat its value as correct and ignore changes from uvars1.
    uvars2.set(L!("lambda"), EnvVar::new(L!("1").to_owned(), noflags)); // same value
    uvars2.set(L!("kappa"), EnvVar::new(L!("2").to_owned(), noflags)); // different value

    // Now see what uvars2 sees.
    callbacks.clear();
    uvars2.sync(&mut callbacks);

    // Sort them to get them in a predictable order.
    callbacks.sort_by(|a, b| a.key.cmp(&b.key));

    // Should see exactly three changes.
    assert_eq!(callbacks.len(), 3);
    assert_eq!(callbacks[0].key, L!("alpha"));
    assert_eq!(callbacks[0].val.as_ref().unwrap().as_string(), L!("2"));
    assert_eq!(callbacks[1].key, L!("beta"));
    assert_eq!(callbacks[1].val.as_ref().unwrap().as_string(), L!("1"));
    assert_eq!(callbacks[2].key, L!("delta"));
    assert_eq!(callbacks[2].val, None);
    std::fs::remove_dir_all("test/fish_uvars_test/").unwrap();
}

#[test]
#[serial]
fn test_universal_formats() {
    let _cleanup = test_init();
    macro_rules! validate {
        ( $version_line:literal, $expected_format:expr ) => {
            assert_eq!(
                EnvUniversal::format_for_contents($version_line),
                $expected_format
            );
        };
    }
    validate!(b"# VERSION: 3.0", UvarFormat::fish_3_0);
    validate!(b"# version: 3.0", UvarFormat::fish_2_x);
    validate!(b"# blah blahVERSION: 3.0", UvarFormat::fish_2_x);
    validate!(b"stuff\n# blah blahVERSION: 3.0", UvarFormat::fish_2_x);
    validate!(b"# blah\n# VERSION: 3.0", UvarFormat::fish_3_0);
    validate!(b"# blah\n#VERSION: 3.0", UvarFormat::fish_3_0);
    validate!(b"# blah\n#VERSION:3.0", UvarFormat::fish_3_0);
    validate!(b"# blah\n#VERSION:3.1", UvarFormat::future);
}

#[test]
#[serial]
fn test_universal_ok_to_save() {
    let _cleanup = test_init();
    // Ensure we don't try to save after reading from a newer fish.
    std::fs::create_dir_all("test/fish_uvars_test/").unwrap();
    let contents = b"# VERSION: 99999.99\n";
    std::fs::write(wcs2osstring(UVARS_TEST_PATH), contents).unwrap();

    let before_id = file_id_for_path(UVARS_TEST_PATH);
    assert_ne!(
        before_id, INVALID_FILE_ID,
        "UVARS_TEST_PATH should be readable"
    );

    let mut cbs = CallbackDataList::new();
    let mut uvars = EnvUniversal::new();
    uvars.initialize_at_path(&mut cbs, UVARS_TEST_PATH.to_owned());
    assert!(!uvars.is_ok_to_save(), "Should not be OK to save");
    uvars.sync(&mut cbs);
    cbs.clear();
    assert!(!uvars.is_ok_to_save(), "Should still not be OK to save");
    uvars.set(
        L!("SOMEVAR"),
        EnvVar::new(L!("SOMEVALUE").to_owned(), EnvVarFlags::empty()),
    );

    // Ensure file is same.
    let after_id = file_id_for_path(UVARS_TEST_PATH);
    assert_eq!(
        before_id, after_id,
        "UVARS_TEST_PATH should not have changed",
    );
    std::fs::remove_dir_all("test/fish_uvars_test/").unwrap();
}
