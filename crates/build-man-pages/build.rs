#[cfg(not(clippy))]
use std::path::Path;

fn main() {
    let mandir = fish_build_helper::fish_build_dir().join("fish-man");
    let sec1dir = mandir.join("man1");
    // Running `cargo clippy` on a clean build directory panics, because when rust-embed tries to
    // embed a directory which does not exist it will panic.
    let _ = std::fs::create_dir_all(sec1dir.to_str().unwrap());

    #[cfg(not(clippy))]
    build_man(&mandir);
}

#[cfg(not(clippy))]
fn build_man(man_dir: &Path) {
    use std::{
        env,
        process::{Command, Stdio},
    };

    use fish_build_helper::workspace_root;

    let workspace_root = workspace_root();

    let man_str = man_dir.to_str().unwrap();

    let sec1_dir = man_dir.join("man1");
    let sec1_str = sec1_dir.to_str().unwrap();

    let docsrc_dir = workspace_root.join("doc_src");
    let docsrc_str = docsrc_dir.to_str().unwrap();

    let sphinx_doc_sources = [
        workspace_root.join("CHANGELOG.rst"),
        workspace_root.join("CONTRIBUTING.rst"),
        docsrc_dir.clone(),
    ];
    fish_build_helper::rebuild_if_paths_changed(sphinx_doc_sources);

    let args = &[
        "-j", "auto", "-q", "-b", "man", "-c", docsrc_str,
        // doctree path - put this *above* the man1 dir to exclude it.
        // this is ~6M
        "-d", man_str, docsrc_str, sec1_str,
    ];
    let _ = std::fs::create_dir_all(sec1_str);

    rsconf::rebuild_if_env_changed("FISH_BUILD_DOCS");
    if env::var("FISH_BUILD_DOCS") == Ok("0".to_string()) {
        rsconf::warn!("Skipping man pages because $FISH_BUILD_DOCS is set to 0");
        return;
    }

    // We run sphinx to build the man pages.
    // Every error here is fatal so cargo doesn't cache the result
    // - if we skipped the docs with sphinx not installed, installing it would not then build the docs.
    // That means you need to explicitly set $FISH_BUILD_DOCS=0 (`FISH_BUILD_DOCS=0 cargo install --path .`),
    // which is unfortunate - but the docs are pretty important because they're also used for --help.
    let sphinx_build = match Command::new("sphinx-build")
        .args(args)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
    {
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
            if env::var("FISH_BUILD_DOCS") == Ok("1".to_string()) {
                panic!("Could not find sphinx-build to build man pages.\nInstall sphinx or disable building the docs by setting $FISH_BUILD_DOCS=0.");
            }
            rsconf::warn!("Cannot find sphinx-build to build man pages.");
            rsconf::warn!("If you install it now you need to run `cargo clean` and rebuild, or set $FISH_BUILD_DOCS=1 explicitly.");
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
