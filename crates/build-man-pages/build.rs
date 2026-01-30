use std::path::Path;

use fish_build_helper::{as_os_strs, fish_doc_dir};

fn main() {
    let sec1_dir = fish_doc_dir().join("man").join("man1");
    // Running `cargo clippy` on a clean build directory panics, because when rust-embed
    // tries to embed a directory which does not exist it will panic.
    let _ = std::fs::create_dir_all(&sec1_dir);
    if !cfg!(clippy) {
        build_man(&sec1_dir);
    }
}

fn build_man(sec1_dir: &Path) {
    use fish_build_helper::{env_var, workspace_root};
    use std::process::{Command, Stdio};

    let workspace_root = workspace_root();
    let doc_src_dir = workspace_root.join("doc_src");
    let doctrees_dir = fish_doc_dir().join(".doctrees-man");

    fish_build_helper::rebuild_if_paths_changed([
        &workspace_root.join("CHANGELOG.rst"),
        &workspace_root.join("CONTRIBUTING.rst"),
        &doc_src_dir,
    ]);

    let args = as_os_strs![
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
        &doctrees_dir,
        &doc_src_dir,
        &sec1_dir,
    ];

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
            assert_ne!(
                env_var("FISH_BUILD_DOCS"),
                Some("1".to_string()),
                "Could not find sphinx-build required to build man pages.\n\
                 Install Sphinx or disable building the docs by setting $FISH_BUILD_DOCS=0."
            );
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
            assert!(
                out.status.success(),
                "sphinx-build failed to build the man pages."
            );
        }
    }
}
