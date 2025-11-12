use fish_build_helper::{env_var, workspace_root};
use std::path::Path;

fn main() {
    let man_dir = fish_build_helper::fish_build_dir().join("fish-man");
    let sec1_dir = man_dir.join("man1");
    // Running `cargo clippy` on a clean build directory panics, because when rust-embed tries to
    // embed a directory which does not exist it will panic.
    let _ = std::fs::create_dir_all(&sec1_dir);

    let help_sections_path = Path::new(&env_var("OUT_DIR").unwrap()).join("help_sections.rs");

    if env_var("FISH_USE_PREBUILT_DOCS").is_some_and(|v| v == "TRUE") {
        std::fs::copy(
            workspace_root().join("user_doc/src/help_sections.rs"),
            help_sections_path,
        )
        .unwrap();
        return;
    }

    std::fs::write(
        help_sections_path.clone(),
        r#"pub static HELP_SECTIONS: &str = "";"#,
    )
    .unwrap();

    #[cfg(not(clippy))]
    build_man(&man_dir, &sec1_dir, &help_sections_path);
}

#[cfg(not(clippy))]
fn build_man(man_dir: &Path, sec1_dir: &Path, help_sections_path: &Path) {
    use std::{
        ffi::OsStr,
        process::{Command, Stdio},
    };

    let workspace_root = workspace_root();
    let doc_src_dir = workspace_root.join("doc_src");

    fish_build_helper::rebuild_if_paths_changed([
        &workspace_root.join("CHANGELOG.rst"),
        &workspace_root.join("CONTRIBUTING.rst"),
        &doc_src_dir,
    ]);

    let help_sections_arg = format!("fish_help_sections_output={}", help_sections_path.display());
    let args: &[&OsStr] = {
        fn as_os_str<S: AsRef<OsStr> + ?Sized>(s: &S) -> &OsStr {
            s.as_ref()
        }
        macro_rules! as_os_strs {
            ( [ $( $x:expr, )* ] ) => {
                &[
                    $( as_os_str($x), )*
                ]
            }
        }
        as_os_strs!([
            "-j",
            "auto",
            "-q",
            "-b",
            "man",
            "-c",
            &doc_src_dir,
            // doctree path - put this *above* the man1 dir to exclude it.
            // this is ~6M
            "-d",
            &man_dir,
            &doc_src_dir,
            &sec1_dir,
            "-D",
            &help_sections_arg,
        ])
    };

    rsconf::rebuild_if_env_changed("FISH_BUILD_DOCS");
    if env_var("FISH_BUILD_DOCS") == Some("0".to_string()) {
        rsconf::warn!("Skipping man pages because $FISH_BUILD_DOCS is set to 0");
        return;
    }

    // We run sphinx to build the man pages.
    // Every error here is fatal so cargo doesn't cache the result
    // - if we skipped the docs with sphinx not installed, installing it would not then build the docs.
    // That means you need to explicitly set $FISH_BUILD_DOCS=0 (`FISH_BUILD_DOCS=0 cargo install --path .`),
    // which is unfortunate - but the docs are pretty important because they're also used for --help.
    let sphinx_build = match Command::new(option_env!("FISH_SPHINX").unwrap_or("sphinx-build"))
        .args(args)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
    {
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
            if env_var("FISH_BUILD_DOCS") == Some("1".to_string()) {
                panic!(
                    "Could not find sphinx-build required to build man pages.\n\
                     Install Sphinx or disable building the docs by setting $FISH_BUILD_DOCS=0."
                );
            }
            rsconf::warn!(
                "Could not find sphinx-build required to build man pages. \
                 If you install Sphinx now, you need to trigger a rebuild to include man pages. \
                 For example by running `touch doc_src` followed by the build command."
            );
            return;
        }
        Err(e) => {
            // Another error - permissions wrong etc
            panic!("Error starting sphinx-build to build man pages: {:?}", e);
        }
        Ok(sphinx_build) => sphinx_build,
    };

    match sphinx_build.wait_with_output() {
        Err(err) => {
            panic!(
                "Error waiting for sphinx-build to build man pages: {:?}",
                err
            );
        }
        Ok(out) => {
            if !out.stderr.is_empty() {
                rsconf::warn!("sphinx-build: {}", String::from_utf8_lossy(&out.stderr));
            }
            assert_eq!(&String::from_utf8_lossy(&out.stdout), "");
            if !out.status.success() {
                panic!("sphinx-build failed to build the man pages.");
            }
        }
    }
}
