use crate::{CARGO, CommandExt, files_with_extension};
use anyhow::{Context as _, Result, bail};
use clap::{Args, Subcommand};
use fish_build_helper::po_dir;
use pcre2::bytes::Regex;
use std::{
    fs::OpenOptions,
    io::{Write as _, stdout},
    path::{Path, PathBuf},
    process::Command,
    thread::spawn,
};

#[derive(Args)]
pub struct GettextArgs {
    /// Path to the directory into which the messages from the Rust sources have been extracted.
    /// If this is not specified, fish will be compiled with the `gettext-extract` feature to
    /// obtain the messages.
    #[arg(long)]
    rust_extraction_dir: Option<PathBuf>,

    #[command(subcommand)]
    task: Task,
}

#[derive(Subcommand)]
enum Task {
    /// Check whether the PO files are up to date.
    /// Prints a diff and exits non-zero if they are outdated.
    /// Considers all our PO files by default, also allows explicitly specifying which files to
    /// consider.
    Check { paths: Vec<PathBuf> },
    /// Add a PO file for a new language.
    New {
        /// An ISO 639-1 language identifier (ISO 639-2 if the former does not exits),
        /// optionally followed by and underscore and an ISO 3166-1 country code to specify the variant,
        /// e.g. `de` or `pt_BR`.
        language: String,
    },
    /// Update PO files.
    /// This will delete entries for msgids which are no longer used in the sources and introduce
    /// new, untranslated entries for messages which do not have an entry yet.
    /// This tool should run every time a change to the messages localized via gettext occurs,
    /// including fish script files, where many strings are implicitly localized.
    /// Considers all our PO files by default, also allows explicitly specifying which files to
    /// consider.
    Update { paths: Vec<PathBuf> },
}

fn get_po_paths<P: AsRef<Path>>(specified_paths: &[P]) -> Result<Vec<PathBuf>> {
    let extension = "po";
    if specified_paths.is_empty() {
        files_with_extension([po_dir()], extension)
    } else {
        files_with_extension(specified_paths, extension)
    }
}

fn update_po_file<P: AsRef<Path>, Q: AsRef<Path>>(file_to_update: P, template: Q) -> Result<()> {
    Command::new("msgmerge")
        .args([
            "--no-wrap",
            "--update",
            "--no-fuzzy-matching",
            "--backup=none",
            "--quiet",
        ])
        .arg(file_to_update.as_ref())
        .arg(template.as_ref())
        .run()?;
    let msgattrib_output_file = fish_tempfile::new_file().context("Failed to create temp file")?;
    Command::new("msgattrib")
        .args(["--no-wrap", "--no-obsolete"])
        .arg("-o")
        .arg(msgattrib_output_file.path())
        .arg(file_to_update.as_ref())
        .run()?;
    crate::copy_file(msgattrib_output_file.path(), file_to_update.as_ref())?;
    Ok(())
}

pub fn gettext(args: GettextArgs) -> Result<()> {
    let template = match args.rust_extraction_dir {
        Some(dir) => template::Template::new(dir)?,
        None => {
            let temp_dir = fish_tempfile::new_dir().context("Failed to create temp file")?;
            Command::new(CARGO)
                .args([
                    "check",
                    "--workspace",
                    "--all-targets",
                    "--features=gettext-extract",
                ])
                .env("FISH_GETTEXT_EXTRACTION_DIR", temp_dir.path())
                .run()?;
            template::Template::new(temp_dir.path())?
        }
    };
    let mut template_file = fish_tempfile::new_file().context("Failed to create temp file")?;
    template_file
        .get_mut()
        .write_all(template.serialize())
        .with_context(|| format!("Failed to write to temp file {:?}", template_file.path()))?;
    template_file
        .get_mut()
        .flush()
        .with_context(|| format!("Failed to flush temporary file {:?}", template_file.path()))?;

    match args.task {
        Task::Check { paths } => {
            let mut thread_handles = vec![];
            for path in get_po_paths(&paths)? {
                let template_path_buf = template_file.path().to_owned();
                let handle = spawn(move || -> Result<Option<Vec<u8>>> {
                    let tmp_copy =
                        fish_tempfile::new_file().context("Failed to create temp file")?;
                    crate::copy_file(&path, tmp_copy.path())?;
                    update_po_file(tmp_copy.path(), template_path_buf)?;
                    let diff_output = Command::new("diff")
                        .arg("-u")
                        .arg(&path)
                        .arg(tmp_copy.path())
                        .output()
                        .context("Failed to run diff")?;
                    if diff_output.status.success() {
                        Ok(None)
                    } else {
                        Ok(Some(diff_output.stdout))
                    }
                });
                thread_handles.push(handle);
            }
            let mut found_diff = false;
            let mut error = None;
            for handle in thread_handles {
                // SAFETY: `handle.join()` only returns `Err` if the thread panicked.
                // Our threads should not panic, and if they do, it's OK to deal with the unexpected
                // behavior by panicking here as well.
                match handle.join().unwrap() {
                    Ok(None) => {}
                    Ok(Some(diff)) => {
                        found_diff = true;
                        stdout()
                            .write_all(&diff)
                            .context("Could not write to stdout")?;
                    }
                    Err(e) => {
                        error = Some(e);
                    }
                }
            }
            if let Some(e) = error {
                return Err(e);
            }
            if found_diff {
                bail!(
                    "Not all PO files are up to date.\n\
                     Run `cargo xtask gettext update` to bring them up to date automatically.\
                    "
                );
            }
            Ok(())
        }
        Task::New { language } => {
            let language_regex = Regex::new("^[a-z]{2,3}(_[A-Z]{2})?$").unwrap();
            if !language_regex.is_match(language.as_bytes()).unwrap() {
                bail!(
                    "The language name '{language}' does not match the expected format.\n\
                    It needs to be a two-letter ISO 639-1 language code, \
                    or a three-letter ISO 639-2 language code \
                    if no ISO 639-1 code exists for the language.\n\
                    Optionally, the language code can be followed be an underscore \
                    followed by an ISO 3166-1 country code to indicate a regional variant.\n\
                    Check the existing file names in {:?} for examples.",
                    po_dir()
                );
            }
            // TODO (MSRV>=1.91): use with_added_extension instead of with_extension
            let po_path = po_dir().join(&language).with_extension("po");
            let mut new_po_file = OpenOptions::new()
                .create_new(true)
                .write(true)
                .open(&po_path)
                .with_context(|| format!("Failed to create file at {po_path:?}"))?;
            let mut header = String::new();
            let line_prefix = "# fish-note-sections: ";
            let lines = [
                "Translations are divided into sections, each starting with a fish-section-* pseudo-message.",
                "The first few sections are more important.",
                "Ignore the tier3 sections unless you have a lot of time.",
            ];
            for line in lines {
                use std::fmt::Write as _;
                let _ = writeln!(header, "{line_prefix}{line}");
            }
            new_po_file
                .write_all(header.as_bytes())
                .with_context(|| format!("Failed to write to {po_path:?}"))?;
            new_po_file
                .write_all(template.serialize())
                .with_context(|| format!("Failed to write to {po_path:?}"))?;
            Ok(())
        }
        Task::Update { paths } => {
            let mut thread_handles = vec![];
            for path in get_po_paths(&paths)? {
                let template_path_buf = template_file.path().to_owned();
                let handle =
                    spawn(move || -> Result<()> { update_po_file(path, template_path_buf) });
                thread_handles.push(handle);
            }
            let mut error = None;
            for handle in thread_handles {
                // SAFETY: `handle.join()` only returns `Err` if the thread panicked.
                // Our threads should not panic, and if they do, it's OK to deal with the unexpected
                // behavior by panicking here as well.
                if let Err(e) = handle.join().unwrap() {
                    error = Some(e);
                }
            }
            if let Some(e) = error {
                return Err(e);
            }
            Ok(())
        }
    }
}

mod template {
    use crate::{CommandExt as _, files_with_extension};
    use anyhow::{Context as _, Result, bail};
    use fish_build_helper::workspace_root;
    use fish_common::{UnescapeFlags, unescape_string};
    use fish_widestring::{str2wcstring, wcs2bytes};
    use pcre2::bytes::Regex;
    use std::{
        collections::{HashMap, HashSet},
        fmt::Display,
        fs::OpenOptions,
        io::Read as _,
        path::Path,
        process::Command,
        sync::LazyLock,
    };

    // Gettext tools require this header to know which encoding is used.
    const MINIMAL_HEADER: &str = r#"msgid ""
msgstr "Content-Type: text/plain; charset=UTF-8\n"

"#;

    #[derive(PartialEq, Eq, PartialOrd, Ord, Clone, Copy, Hash)]
    enum LocalizationTier {
        Tier1,
        Tier2,
        Tier3,
    }

    impl TryFrom<&str> for LocalizationTier {
        type Error = ();

        fn try_from(value: &str) -> Result<Self, Self::Error> {
            match value {
                "tier1" => Ok(Self::Tier1),
                "tier2" => Ok(Self::Tier2),
                "tier3" => Ok(Self::Tier3),
                _ => Err(()),
            }
        }
    }

    impl Display for LocalizationTier {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            f.write_str(match self {
                Self::Tier1 => "tier1",
                Self::Tier2 => "tier2",
                Self::Tier3 => "tier3",
            })
        }
    }

    #[derive(Default)]
    struct FishScriptMessages {
        explicit: HashSet<String>,
        implicit: HashSet<String>,
    }

    pub struct Template {
        content: Vec<u8>,
    }

    impl Template {
        pub fn serialize(&self) -> &[u8] {
            &self.content
        }

        /// Create a gettext template.
        /// `rust_extraction_dir` must be the path to a directory which contains the messages
        /// extracted from the Rust sources.
        pub fn new<P: AsRef<Path>>(rust_extraction_dir: P) -> Result<Self> {
            let mut template = Self {
                content: Vec::from(MINIMAL_HEADER.as_bytes()),
            };
            template.add_rust_messages(rust_extraction_dir)?;
            template.add_fish_script_messages()?;
            // TODO: keep internal set of msgids to avoid having to run msguniq. requires parsing
            // gettext-extraction output
            let msguniq_output = Command::new("msguniq")
                .args(["--no-wrap"])
                .run_with_stdio(template.content)?;
            Ok(Template {
                content: msguniq_output,
            })
        }

        /// Expects `extraction_dir` to contain only files whose content are single PO entries which can be
        /// concatenated into a valid PO file.
        /// If this is the case, the messages are de-duplicated and sorted by `msguniq`.
        /// The result is appended to `template`, with a leading section marker.
        /// On failure, the process aborts.
        fn add_rust_messages<P: AsRef<Path>>(&mut self, extraction_dir: P) -> Result<()> {
            let extraction_dir = extraction_dir.as_ref();
            let mut concatenated_content = Vec::from(MINIMAL_HEADER.as_bytes());

            // Concatenate the content of all files in `extraction_dir` into `concatenated_content`.
            for entry_result in extraction_dir
                .read_dir()
                .with_context(|| format!("Failed to read directory {extraction_dir:?}"))?
            {
                let entry = entry_result
                    .with_context(|| format!("Failed to get entry in {extraction_dir:?}"))?;
                let entry_path = entry.path();
                if !entry
                    .file_type()
                    .with_context(|| format!("Failed to get file type of {entry_path:?}"))?
                    .is_file()
                {
                    bail!("Entry in {extraction_dir:?} is not a regular file");
                }
                let mut file = OpenOptions::new()
                    .read(true)
                    .open(&entry_path)
                    .with_context(|| format!("Failed to open file {entry_path:?}"))?;
                file.read_to_end(&mut concatenated_content)
                    .with_context(|| format!("Failed to read file {entry_path:?}"))?;
            }

            // Get rid of duplicates and sort.
            let msguniq_output = Command::new("msguniq")
                .args(["--no-wrap", "--sort-output"])
                .env("LC_ALL", "C.UTF-8")
                .run_with_stdio(concatenated_content)?;
            // The Header entry needs to be removed again,
            // because it is added outside of this function.
            let expected_prefix = MINIMAL_HEADER.as_bytes();
            let actual_prefix = &msguniq_output[..expected_prefix.len()];
            if expected_prefix != actual_prefix {
                bail!(
                    "Prefix of msguniq output does not match expected header.\nExpected bytes:\n{expected_prefix:02x?}\nActual bytes:\n{actual_prefix:02x?}"
                );
            }
            self.mark_section("tier1-from-rust");
            self.content
                .extend_from_slice(&msguniq_output[expected_prefix.len()..]);
            self.content.push(b'\n');
            Ok(())
        }

        fn mark_section(&mut self, section_name: &str) {
            self.content
                .extend_from_slice("msgid \"fish-section-".as_bytes());
            self.content.extend_from_slice(section_name.as_bytes());
            self.content
                .extend_from_slice("\"\nmsgstr \"\"\n\n".as_bytes());
        }

        fn append_messages(&mut self, msgids: &HashSet<String>) -> Result<()> {
            let mut unescaped_msgids = HashSet::new();
            for msgid in msgids {
                let unescaped_wstring = unescape_string(
                    &str2wcstring(msgid),
                    fish_common::UnescapeStringStyle::Script(UnescapeFlags::default()),
                )
                .with_context(|| format!("Failed to unescape the following string:\n{msgid}"))?;
                unescaped_msgids.insert(
                    String::from_utf8(wcs2bytes(&unescaped_wstring))
                        .context("Parsed msgid is not valid UTF-8")?,
                );
            }
            let mut unescaped_msgids = Vec::from_iter(unescaped_msgids);
            unescaped_msgids.sort();
            for msgid in &unescaped_msgids {
                self.content
                    .extend_from_slice(format_msgid_for_po(msgid).as_bytes());
            }
            Ok(())
        }

        fn add_script_tier(
            &mut self,
            tier: LocalizationTier,
            messages: FishScriptMessages,
        ) -> Result<()> {
            if !messages.explicit.is_empty() {
                self.mark_section(&format!("{tier}-from-script-explicitly-added"));
                self.append_messages(&messages.explicit)?;
            }
            if !messages.implicit.is_empty() {
                self.mark_section(&format!("{tier}-from-script-implicitly-added"));
                self.append_messages(&messages.implicit)?;
            }
            Ok(())
        }

        fn add_fish_script_messages(&mut self) -> Result<()> {
            let share_dir = workspace_root().join("share");
            let relevant_file_paths = files_with_extension(
                [
                    share_dir.join("config.fish"),
                    share_dir.join("completions"),
                    share_dir.join("functions"),
                ],
                "fish",
            )?;
            let mut extracted_messages = HashMap::new();
            for path in relevant_file_paths {
                extract_messages_from_fish_script(path, &mut extracted_messages)?;
            }
            let mut messages_sorted_by_tier: Vec<_> = extracted_messages.into_iter().collect();
            messages_sorted_by_tier.sort_by_key(|(tier, _)| *tier);
            for (tier, messages) in messages_sorted_by_tier {
                self.add_script_tier(tier, messages)?;
            }
            Ok(())
        }
    }

    fn find_localization_tier<P: AsRef<Path>>(
        input: &str,
        path: P,
    ) -> Result<Option<LocalizationTier>> {
        static L10N_ANNOTATION: LazyLock<Regex> = LazyLock::new(|| {
            Regex::new(r"(?:^|\n)# localization: (?<annotation_value>.*)\n").unwrap()
        });
        if let Some(annotation) = L10N_ANNOTATION.captures(input.as_bytes()).unwrap() {
            // SAFETY: `tier` is the name of a capture group in the regex whose captures we are looking
            // at. The capture is done on the bytes of an UTF-8 encoded string, so the result will also
            // be UTF-8 encoded, and the sub-slice we are looking at will start and end at codepoint
            // boundaries.
            let annotation_value =
                std::str::from_utf8(annotation.name("annotation_value").unwrap().as_bytes())
                    .unwrap();
            if let Ok(tier) = LocalizationTier::try_from(annotation_value) {
                return Ok(Some(tier));
            }
            if annotation_value.starts_with("skip") {
                return Ok(None);
            }
            bail!(
                "Unexpected localization annotation in file {:?}: {annotation_value}",
                path.as_ref()
            );
        }
        let dirname = path
            .as_ref()
            .parent()
            .with_context(|| {
                format!(
                    "Tried to get the parent of a path which does not have a parent: {:?}",
                    path.as_ref()
                )
            })?
            .file_name()
            .with_context(|| {
                format!(
                    "The parent of {:?} does not have a filename component",
                    path.as_ref()
                )
            })?;
        let command_name = path
            .as_ref()
            .file_stem()
            .with_context(|| format!("The path {:?} does not have a file stem", path.as_ref()))?;
        if dirname == "functions"
            && command_name
                .to_str()
                .is_some_and(|name| name.starts_with("fish_"))
        {
            return Ok(Some(LocalizationTier::Tier1));
        }
        if dirname != "completions" {
            bail!(
                "Missing localization tier for function file {:?}",
                path.as_ref()
            );
        }
        // TODO (MSRV>=1.91): use with_added_extension instead of with_extension
        let doc_path = workspace_root()
            .join("doc_src")
            .join("cmds")
            .join(command_name)
            .with_extension("rst");
        let doc_path_exists = std::fs::exists(&doc_path)
            .with_context(|| format!("Failed to check whether a file exists at {doc_path:?}"))?;
        Ok(Some(if doc_path_exists {
            LocalizationTier::Tier1
        } else {
            LocalizationTier::Tier3
        }))
    }

    fn extract_messages_from_fish_script<P: AsRef<Path>>(
        path: P,
        extracted_messages: &mut HashMap<LocalizationTier, FishScriptMessages>,
    ) -> Result<()> {
        let path = path.as_ref();
        let file_content = std::fs::read_to_string(path)
            .with_context(|| format!("Failed to read from {path:?}"))?;
        let Some(tier) = find_localization_tier(&file_content, path)? else {
            return Ok(());
        };
        // TODO: use proper parser instead of regex
        static EXPLICIT_MESSAGE: LazyLock<Regex> =
            LazyLock::new(|| Regex::new(r#"\( *_ (?<message>(['"]).+?(?<!\\)\2) *\)"#).unwrap());
        static IMPLICIT_MESSAGE: LazyLock<Regex> = LazyLock::new(|| {
            Regex::new(r#"(?:^|\n)(?:\s|and |or )*(?:complete|function).*? (?:-d|--description) (?<message>(['"]).+?(?<!\\)\2)"#).unwrap()
        });
        let messages_at_tier = extracted_messages.entry(tier).or_default();
        for message in EXPLICIT_MESSAGE.captures_iter(file_content.as_bytes()) {
            let message =
                std::str::from_utf8(message.unwrap().name("message").unwrap().as_bytes()).unwrap();
            messages_at_tier.explicit.insert(message.to_owned());
        }
        for message in IMPLICIT_MESSAGE.captures_iter(file_content.as_bytes()) {
            let message =
                std::str::from_utf8(message.unwrap().name("message").unwrap().as_bytes()).unwrap();
            messages_at_tier.implicit.insert(message.to_owned());
        }
        Ok(())
    }

    fn format_msgid_for_po(msgid: &str) -> String {
        let escaped_msgid = msgid.replace("\\", "\\\\").replace("\"", "\\\"");
        format!(
            "\
            msgid \"{escaped_msgid}\"\n\
            msgstr \"\"\n\
            \n\
            "
        )
    }
}
