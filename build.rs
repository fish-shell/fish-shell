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

    // safety: single-threaded code.
    unsafe { std::env::set_var("FISH_BUILD_VERSION", version) };

    fish_build_helper::rebuild_if_embedded_path_changed("share");

    let build = cc::Build::new();
    let mut target = Target::new_from(build).unwrap();
    // Keep verbose mode on until we've ironed out rust build script stuff
    target.set_verbose(true);
    detect_cfgs(&mut target);

    #[cfg(all(target_env = "gnu", target_feature = "crt-static"))]
    compile_error!(
        "Statically linking against glibc has unavoidable crashes and is unsupported. Use dynamic linking or link statically against musl."
    );
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
fn detect_cfgs(target: &mut Target) {
    for (name, handler) in [
        // Ignore the first entry, it just sets up the type inference.
        ("", &(|_: &Target| false) as &dyn Fn(&Target) -> bool),
        ("apple", &detect_apple),
        ("bsd", &detect_bsd),
        ("cygwin", &detect_cygwin),
        ("have_eventfd", &|target| {
            // FIXME: NetBSD 10 has eventfd, but the libc crate does not expose it.
            if target_os() == "netbsd" {
                false
            } else {
                target.has_header("sys/eventfd.h")
            }
        }),
        ("have_localeconv_l", &|target| {
            target.has_symbol("localeconv_l")
        }),
        ("have_pipe2", &|target| target.has_symbol("pipe2")),
        ("have_posix_spawn", &|target| {
            if matches!(target_os().as_str(), "openbsd" | "android") {
                // OpenBSD's posix_spawn returns status 127 instead of erroring with ENOEXEC when faced with a
                // shebang-less script. Disable posix_spawn on OpenBSD.
                //
                // Android is broken for unclear reasons
                false
            } else {
                target.has_header("spawn.h")
            }
        }),
        ("small_main_stack", &has_small_stack),
        ("use_prebuilt_docs", &|_| {
            env_var("FISH_USE_PREBUILT_DOCS").is_some_and(|v| v == "TRUE")
        }),
        ("using_cmake", &|_| {
            option_env!("FISH_CMAKE_BINARY_DIR").is_some()
        }),
        ("waitstatus_signal_ret", &|target| {
            target.r#if("WEXITSTATUS(0x007f) == 0x7f", &["sys/wait.h"])
        }),
    ] {
        rsconf::declare_cfg(name, handler(target))
    }
}

// Target OS for compiling our crates, as opposed to the build script.
fn target_os() -> String {
    env_var("CARGO_CFG_TARGET_OS").unwrap()
}

fn detect_apple(_: &Target) -> bool {
    matches!(target_os().as_str(), "ios" | "macos")
}

fn detect_cygwin(_: &Target) -> bool {
    target_os() == "cygwin"
}

/// Detect if we're being compiled for a BSD-derived OS, allowing targeting code conditionally with
/// `#[cfg(bsd)]`.
///
/// Rust offers fine-grained conditional compilation per-os for the popular operating systems, but
/// doesn't necessarily include less-popular forks nor does it group them into families more
/// specific than "windows" vs "unix" so we can conditionally compile code for BSD systems.
fn detect_bsd(_: &Target) -> bool {
    let target_os = target_os();
    let is_bsd = target_os.ends_with("bsd") || target_os == "dragonfly";
    if matches!(
        target_os.as_str(),
        "dragonfly" | "freebsd" | "netbsd" | "openbsd"
    ) {
        assert!(is_bsd, "Target incorrectly detected as not BSD!");
    }
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

        unsafe extern "C" {
            unsafe fn pthread_get_stacksize_np(thread: *const ffi::c_void) -> usize;
            unsafe fn pthread_self() -> *const ffi::c_void;
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

    fn overridable_path(
        env_var_name: &str,
        f: impl FnOnce(Option<String>) -> Option<PathBuf>,
    ) -> Option<PathBuf> {
        rsconf::rebuild_if_env_changed(env_var_name);
        let maybe_path = f(env_var(env_var_name));
        if let Some(path) = maybe_path.as_ref() {
            rsconf::set_env_value(env_var_name, path.to_str().unwrap());
        }
        maybe_path
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
        Some(PathBuf::from(
            env_prefix.unwrap_or("/usr/local".to_string()),
        ))
    })
    .unwrap();

    overridable_path("SYSCONFDIR", |env_sysconfdir| {
        Some(join_if_relative(
            &prefix,
            env_sysconfdir.unwrap_or("/etc/".to_string()),
        ))
    });

    let datadir = overridable_path("DATADIR", |env_datadir| {
        env_datadir.map(|p| join_if_relative(&prefix, p))
    });
    overridable_path("BINDIR", |env_bindir| {
        env_bindir.map(|p| join_if_relative(&prefix, p))
    });
    overridable_path("DOCDIR", |env_docdir| {
        env_docdir.map(|p| {
            join_if_relative(
                &datadir
                    .expect("Setting DOCDIR without setting DATADIR is not currently supported"),
                p,
            )
        })
    });
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
