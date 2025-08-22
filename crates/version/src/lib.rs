use std::{fs::read_to_string, path::Path, process::Command};

pub fn get_version() -> String {
    if let Ok(var) = std::env::var("FISH_BUILD_VERSION") {
        return var;
    }

    let repo_root_dir = fish_build_helper::get_repo_root();
    let path = repo_root_dir.join("version");
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
    fn get_git_hash(repo_root_dir: &Path) -> Result<String, Box<dyn std::error::Error>> {
        let gitdir = repo_root_dir.join(".git");
        let jjdir = repo_root_dir.join(".jj");
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

    get_git_hash(&repo_root_dir)
        .expect("Could not get a version. Either set $FISH_BUILD_VERSION or install git.")
}
