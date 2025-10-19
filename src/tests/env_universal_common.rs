use crate::common::ENCODE_DIRECT_BASE;
use crate::common::char_offset;
use crate::common::wcs2osstring;
use crate::env::{EnvVar, EnvVarFlags, VarTable};
use crate::env_universal_common::{EnvUniversal, UvarFormat};
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use crate::wutil::{INVALID_FILE_ID, file_id_for_path};

const UVARS_PER_THREAD: usize = 8;

/// Creates a unique temporary directory and file path for universal variable tests.
/// Returns (directory_path, file_path).
fn make_test_uvar_path(test_name: &str) -> (std::path::PathBuf, WString) {
    let test_dir = std::env::temp_dir().join(format!(
        "fish_test_{}_{:?}",
        test_name,
        std::thread::current().id()
    ));
    let test_path = sprintf!("%s/varsfile.txt", test_dir.to_string_lossy());
    (test_dir, test_path)
}

fn test_universal_helper(x: usize, path: &wstr) {
    let _cleanup = test_init();
    let mut uvars = EnvUniversal::new();
    uvars.initialize_at_path(path.to_owned());

    for j in 0..UVARS_PER_THREAD {
        let key = sprintf!("key_%d_%d", x, j);
        let val = sprintf!("val_%d_%d", x, j);
        uvars.set(&key, EnvVar::new(val, EnvVarFlags::empty()));
        let (synced, _) = uvars.sync();
        assert!(
            synced,
            "Failed to sync universal variables after modification"
        );
    }

    // Last step is to delete the first key.
    uvars.remove(&sprintf!("key_%d_%d", x, 0));
    let (synced, _) = uvars.sync();
    assert!(synced, "Failed to sync universal variables after deletion");
}

#[test]
fn test_universal() {
    let _cleanup = test_init();
    let (test_dir, test_path) = make_test_uvar_path("universal");
    let _ = std::fs::remove_dir_all(&test_dir);
    std::fs::create_dir_all(&test_dir).unwrap();

    let threads = 1;
    let mut handles = Vec::new();

    for i in 0..threads {
        let path = test_path.to_owned();
        handles.push(std::thread::spawn(move || {
            test_universal_helper(i, &path);
        }));
    }

    for h in handles {
        h.join().unwrap();
    }

    let mut uvars = EnvUniversal::new();
    uvars.initialize_at_path(test_path.to_owned());

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

    std::fs::remove_dir_all(&test_dir).unwrap();
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
fn test_universal_callbacks() {
    let _cleanup = test_init();
    let (test_dir, test_path) = make_test_uvar_path("callbacks");
    std::fs::create_dir_all(&test_dir).unwrap();
    let mut uvars1 = EnvUniversal::new();
    let mut uvars2 = EnvUniversal::new();
    let mut callbacks = uvars1
        .initialize_at_path(test_path.to_owned())
        .unwrap_or_default();
    callbacks.append(
        &mut uvars2
            .initialize_at_path(test_path.to_owned())
            .unwrap_or_default(),
    );

    macro_rules! sync {
        ($uvars:expr) => {
            let (_, cb_opt) = $uvars.sync();
            if let Some(mut cb) = cb_opt {
                callbacks.append(&mut cb);
            }
        };
    }

    let noflags = EnvVarFlags::empty();

    // Put some variables into both.
    uvars1.set(L!("alpha"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("beta"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("delta"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("epsilon"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("lambda"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("kappa"), EnvVar::new(L!("1").to_owned(), noflags)); //
    uvars1.set(L!("omicron"), EnvVar::new(L!("1").to_owned(), noflags)); //

    sync!(uvars1);
    sync!(uvars2);

    // Change uvars1.
    uvars1.set(L!("alpha"), EnvVar::new(L!("2").to_owned(), noflags)); // changes value
    uvars1.set(
        L!("beta"),
        EnvVar::new(L!("1").to_owned(), EnvVarFlags::EXPORT),
    ); // changes export
    uvars1.remove(L!("delta")); // erases value
    uvars1.set(L!("epsilon"), EnvVar::new(L!("1").to_owned(), noflags)); // changes nothing
    sync!(uvars1);

    // Change uvars2. It should treat its value as correct and ignore changes from uvars1.
    uvars2.set(L!("lambda"), EnvVar::new(L!("1").to_owned(), noflags)); // same value
    uvars2.set(L!("kappa"), EnvVar::new(L!("2").to_owned(), noflags)); // different value

    // Now see what uvars2 sees.
    callbacks.clear();
    sync!(uvars2);

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
    std::fs::remove_dir_all(&test_dir).unwrap();
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
fn test_universal_ok_to_save() {
    let _cleanup = test_init();
    // Ensure we don't try to save after reading from a newer fish.
    let (test_dir, test_path) = make_test_uvar_path("ok_to_save");
    std::fs::create_dir_all(&test_dir).unwrap();
    let contents = b"# VERSION: 99999.99\n";
    std::fs::write(wcs2osstring(&test_path), contents).unwrap();

    let before_id = file_id_for_path(&test_path);
    assert_ne!(before_id, INVALID_FILE_ID, "test_path should be readable");

    let mut uvars = EnvUniversal::new();
    uvars
        .initialize_at_path(test_path.to_owned())
        .unwrap_or_default();
    assert!(!uvars.is_ok_to_save(), "Should not be OK to save");
    uvars.sync();
    assert!(!uvars.is_ok_to_save(), "Should still not be OK to save");
    uvars.set(
        L!("SOMEVAR"),
        EnvVar::new(L!("SOMEVALUE").to_owned(), EnvVarFlags::empty()),
    );

    // Ensure file is same.
    let after_id = file_id_for_path(&test_path);
    assert_eq!(before_id, after_id, "test_path should not have changed",);
    std::fs::remove_dir_all(&test_dir).unwrap();
}
