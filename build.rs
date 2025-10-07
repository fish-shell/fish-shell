#![allow(clippy::uninlined_format_args)]

use fish_build_helper::{env_var, fish_build_dir, workspace_root};
use rsconf::Target;
use std::env;
use std::path::{Path, PathBuf};

fn canonicalize<P: AsRef<Path>>(path: P) -> PathBuf {
    std::fs::canonicalize(path).unwrap()
}

fn main() {
    setup_paths();

    // Add our default to enable tools that don't go through CMake, like "cargo test" and the
    // language server.

    rsconf::set_env_value(
        "FISH_RESOLVED_BUILD_DIR",
        // If set by CMake, this might include symlinks. Since we want to compare this to the
        // dir fish is executed in we need to canonicalize it.
        canonicalize(fish_build_dir()).to_str().unwrap(),
    );

    // We need to canonicalize (i.e. realpath) the manifest dir because we want to be able to
    // compare it directly as a string at runtime.
    rsconf::set_env_value(
        "CARGO_MANIFEST_DIR",
        canonicalize(workspace_root()).to_str().unwrap(),
    );

    // Some build info
    rsconf::set_env_value("BUILD_TARGET_TRIPLE", &env_var("TARGET").unwrap());
    rsconf::set_env_value("BUILD_HOST_TRIPLE", &env_var("HOST").unwrap());
    rsconf::set_env_value("BUILD_PROFILE", &env_var("PROFILE").unwrap());

    let version = &get_version(&env::current_dir().unwrap());
    // Per https://doc.rust-lang.org/cargo/reference/build-scripts.html#inputs-to-the-build-script,
    // the source directory is the current working directory of the build script
    rsconf::set_env_value("FISH_BUILD_VERSION", version);

    std::env::set_var("FISH_BUILD_VERSION", version);

    // These are necessary if built with embedded functions,
    // but only in release builds (because rust-embed in debug builds reads from the filesystem).
    #[cfg(feature = "embed-data")]
    #[cfg(any(windows, not(debug_assertions)))]
    rsconf::rebuild_if_path_changed("share");

    #[cfg(feature = "gettext-extract")]
    rsconf::rebuild_if_env_changed("FISH_GETTEXT_EXTRACTION_FILE");

    let build = cc::Build::new();
    let mut target = Target::new_from(build).unwrap();
    // Keep verbose mode on until we've ironed out rust build script stuff
    target.set_verbose(true);
    detect_cfgs(&mut target);

    #[cfg(all(target_env = "gnu", target_feature = "crt-static"))]
    compile_error!("Statically linking against glibc has unavoidable crashes and is unsupported. Use dynamic linking or link statically against musl.");
}

/// Check target system support for certain functionality dynamically when the build is invoked,
/// without their having to be explicitly enabled in the `cargo build --features xxx` invocation.
///
/// We are using [`rsconf::enable_cfg()`] instead of [`rsconf::enable_feature()`] as rust features
/// should be used for things that a user can/would reasonably enable or disable to tweak or coerce
/// behavior, but here we are testing for whether or not things are supported altogether.
///
/// This can be used to enable features that we check for and conditionally compile according to in
/// our own codebase, but [can't be used to pull in dependencies](0) even if they're gated (in
/// `Cargo.toml`) behind a feature we just enabled.
///
/// [0]: https://github.com/rust-lang/cargo/issues/5499
#[rustfmt::skip]
fn detect_cfgs(target: &mut Target) {
    for (name, handler) in [
        // Ignore the first entry, it just sets up the type inference. Model new entries after the
        // second line.
        ("", &(|_: &Target| false) as &dyn Fn(&Target) -> bool),
        ("apple", &detect_apple),
        ("bsd", &detect_bsd),
        ("cygwin", &detect_cygwin),
        ("small_main_stack", &has_small_stack),
        // See if libc supports the thread-safe localeconv_l(3) alternative to localeconv(3).
        ("localeconv_l", &|target| {
            target.has_symbol("localeconv_l")
        }),
        ("FISH_USE_POSIX_SPAWN", &|target| {
            target.has_header("spawn.h")
        }),
        ("HAVE_PIPE2", &|target| {
            target.has_symbol("pipe2")
        }),
        ("HAVE_EVENTFD", &|target| {
            // FIXME: NetBSD 10 has eventfd, but the libc crate does not expose it.
            if cfg!(target_os = "netbsd") {
                 false
             } else {
                 target.has_header("sys/eventfd.h")
            }
        }),
        ("HAVE_WAITSTATUS_SIGNAL_RET", &|target| {
            target.r#if("WEXITSTATUS(0x007f) == 0x7f", &["sys/wait.h"])
        }),
    ] {
        rsconf::declare_cfg(name, handler(target))
    }
}

fn detect_apple(_: &Target) -> bool {
    cfg!(any(target_os = "ios", target_os = "macos"))
}

fn detect_cygwin(_: &Target) -> bool {
    // Cygwin target is usually cross-compiled.
    env_var("CARGO_CFG_TARGET_OS").unwrap() == "cygwin"
}

/// Detect if we're being compiled for a BSD-derived OS, allowing targeting code conditionally with
/// `#[cfg(bsd)]`.
///
/// Rust offers fine-grained conditional compilation per-os for the popular operating systems, but
/// doesn't necessarily include less-popular forks nor does it group them into families more
/// specific than "windows" vs "unix" so we can conditionally compile code for BSD systems.
fn detect_bsd(_: &Target) -> bool {
    // Instead of using `uname`, we can inspect the TARGET env variable set by Cargo. This lets us
    // support cross-compilation scenarios.
    let mut target = env_var("TARGET").unwrap();
    if !target.chars().all(|c| c.is_ascii_lowercase()) {
        target = target.to_ascii_lowercase();
    }
    #[allow(clippy::let_and_return)] // for old clippy
    let is_bsd = target.ends_with("bsd") || target.ends_with("dragonfly");
    #[cfg(any(
        target_os = "dragonfly",
        target_os = "freebsd",
        target_os = "netbsd",
        target_os = "openbsd",
    ))]
    assert!(is_bsd, "Target incorrectly detected as not BSD!");
    is_bsd
}

/// Rust sets the stack size of newly created threads to a sane value, but is at at the mercy of the
/// OS when it comes to the size of the main stack. Some platforms we support default to a tiny
/// 0.5 MiB main stack, which is insufficient for fish's MAX_EVAL_DEPTH/MAX_STACK_DEPTH values.
///
/// 0.5 MiB is small enough that we'd have to drastically reduce MAX_STACK_DEPTH to less than 10, so
/// we instead use a workaround to increase the main thread size.
fn has_small_stack(_: &Target) -> bool {
    #[cfg(not(any(target_os = "ios", target_os = "macos", target_os = "netbsd")))]
    return false;

    // NetBSD 10 also needs this but can't find pthread_get_stacksize_np.
    #[cfg(target_os = "netbsd")]
    return true;

    #[cfg(any(target_os = "ios", target_os = "macos"))]
    {
        use core::ffi;

        extern "C" {
            fn pthread_get_stacksize_np(thread: *const ffi::c_void) -> usize;
            fn pthread_self() -> *const ffi::c_void;
        }

        // build.rs is executed on the main thread, so we are getting the main thread's stack size.
        // Modern macOS versions default to an 8 MiB main stack but legacy OS X have a 0.5 MiB one.
        let stack_size = unsafe { pthread_get_stacksize_np(pthread_self()) };
        const TWO_MIB: usize = 2 * 1024 * 1024 - 1;
        stack_size <= TWO_MIB
    }
}

fn setup_paths() {
    #[cfg(windows)]
    use unix_path::{Path, PathBuf};

    fn overridable_path(env_var_name: &str, f: impl FnOnce(Option<String>) -> PathBuf) -> PathBuf {
        rsconf::rebuild_if_env_changed(env_var_name);
        let path = f(env_var(env_var_name));
        rsconf::set_env_value(env_var_name, path.to_str().unwrap());
        path
    }

    fn join_if_relative(parent_if_relative: &Path, path: String) -> PathBuf {
        let path = PathBuf::from(path);
        if path.is_relative() {
            parent_if_relative.join(path)
        } else {
            path
        }
    }

    let prefix = overridable_path("PREFIX", |env_prefix| {
        PathBuf::from(env_prefix.unwrap_or("/usr/local".to_string()))
    });

    let datadir = join_if_relative(&prefix, env_var("DATADIR").unwrap_or("share/".to_string()));
    rsconf::rebuild_if_env_changed("DATADIR");
    #[cfg(not(feature = "embed-data"))]
    rsconf::set_env_value("DATADIR", datadir.to_str().unwrap());

    overridable_path("SYSCONFDIR", |env_sysconfdir| {
        join_if_relative(
            &datadir,
            env_sysconfdir.unwrap_or(
                // Embedded builds use "/etc," not "./share/etc".
                if cfg!(feature = "embed-data") {
                    "/etc/"
                } else {
                    "etc/"
                }
                .to_string(),
            ),
        )
    });

    #[cfg(not(feature = "embed-data"))]
    {
        overridable_path("BINDIR", |env_bindir| {
            join_if_relative(&prefix, env_bindir.unwrap_or("bin/".to_string()))
        });
        overridable_path("LOCALEDIR", |env_localedir| {
            join_if_relative(&datadir, env_localedir.unwrap_or("locale/".to_string()))
        });
        overridable_path("DOCDIR", |env_docdir| {
            join_if_relative(&datadir, env_docdir.unwrap_or("doc/fish".to_string()))
        });
    }
}

fn get_version(src_dir: &Path) -> String {
    use std::fs::read_to_string;
    use std::process::Command;

    if let Some(var) = env_var("FISH_BUILD_VERSION") {
        return var;
    }

    let path = src_dir.join("version");
    if let Ok(strver) = read_to_string(path) {
        return strver;
    }

    let args = &["describe", "--always", "--dirty=-dirty"];
    if let Ok(output) = Command::new("git").args(args).output() {
        let rev = String::from_utf8_lossy(&output.stdout).trim().to_string();
        if !rev.is_empty() {
            // If it contains a ".", we have a proper version like "3.7",
            // or "23.2.1-1234-gfab1234"
            if rev.contains('.') {
                return rev;
            }
            // If it doesn't, we probably got *just* the commit SHA,
            // like "f1242abcdef".
            // So we prepend the crate version so it at least looks like
            // "3.8-gf1242abcdef"
            // This lacks the commit *distance*, but that can't be helped without
            // tags.
            let version = env!("CARGO_PKG_VERSION").to_owned();
            return version + "-g" + &rev;
        }
    }

    // git did not tell us a SHA either because it isn't installed,
    // or because it refused (safe.directory applies to `git describe`!)
    // So we read the SHA ourselves.
    fn get_git_hash() -> Result<String, Box<dyn std::error::Error>> {
        let workspace_root = workspace_root();
        let gitdir = workspace_root.join(".git");
        let jjdir = workspace_root.join(".jj");
        let commit_id = if gitdir.exists() {
            // .git/HEAD contains ref: refs/heads/branch
            let headpath = gitdir.join("HEAD");
            let headstr = read_to_string(headpath)?;
            let headref = headstr.split(' ').nth(1).unwrap().trim();

            // .git/refs/heads/branch contains the SHA
            let refpath = gitdir.join(headref);
            // Shorten to 9 characters (what git describe does currently)
            read_to_string(refpath)?
        } else if jjdir.exists() {
            let output = Command::new("jj")
                .args([
                    "log",
                    "--revisions",
                    "@",
                    "--no-graph",
                    "--ignore-working-copy",
                    "--template",
                    "commit_id",
                ])
                .output()
                .unwrap();
            String::from_utf8_lossy(&output.stdout).to_string()
        } else {
            return Err("did not find either of .git or .jj".into());
        };
        let refstr = &commit_id[0..9];
        let refstr = refstr.trim();

        let version = env!("CARGO_PKG_VERSION").to_owned();
        Ok(version + "-g" + refstr)
    }

    get_git_hash().expect("Could not get a version. Either set $FISH_BUILD_VERSION or install git.")
}
