use crate::files_with_extension;
use anyhow::{Context, Result, anyhow, bail};
use clap::{Args, Subcommand};
use fish_build_helper::{default_ftl_file, ftl_dir};
use fluent_ftl_tools::{
    HasEntries,
    annotate::{
        Annotation, MessageAnnotationRead, MessageAnnotationWrite, add_annotation_to_messages,
        get_message_ids_with_different_value, modify_messages, remove_annotation_from_messages,
    },
    consistency::{check_all_resource_files, remove_inconsistent_translations},
    delete_message_from_paths, filter_resource_messages,
    format::{FormattingMode, format_text},
    format_resource,
    missing::find_missing_message_ids_in_files,
    parse_as_syntax_resource, parse_str_as_syntax_resource,
    rename::rename_in_all_files,
    serialize_resource, serialize_resource_to_file, serialize_resources_to_files,
};
use fluent_syntax::{
    ast::{Entry, Resource},
    serializer::Serializer,
};
use std::{
    collections::{HashMap, HashSet},
    io::{Read, Write},
    path::{Path, PathBuf},
    process::{Command, Stdio},
};

#[derive(Subcommand)]
pub enum FluentCommandArgs {
    /// Check consistency of FTL files.
    Check {
        /// Also check if `en.ftl` is up to date with respect to the Rust sources.
        /// This argument optionally takes a value, which must be the path to the directory into
        /// which the message definitions have been extracted.
        /// If this argument is provided without a value, fish will be recompiled with the `fluent-extract` feature.
        #[arg(long)]
        from_source: Option<Option<PathBuf>>,
        files: Vec<PathBuf>,
    },
    /// Format files. Without arguments, all FTL files are formatted.
    /// If '-' is specified, input is read from stdin and output written to stdout.
    Format { files: Vec<PathBuf> },
    /// Rename an ID or variables used with an ID.
    Rename(RenameArgs),
    /// Resolve outdated translations.
    /// The `update` subcommand of this xtask adds annotations to outdated translations.
    /// These can be resolved manually, but for developers not familiar with the affected languages
    /// can be more convenient to use this tool.
    #[command(subcommand)]
    ResolveOutdated(ResolveOutdatedCommand),
    /// Display message IDs which could be added to the specified file(s), or all files if no
    /// arguments are given.
    ShowMissing { files: Vec<PathBuf> },
    /// Generate en.ftl from the source code and update translations.
    /// New messages will silently be inserted into en.ftl.
    /// Deleted messages will be removed from all FTL files.
    /// If a Fluent variable is renamed or removed, the translations of the message will be deleted.
    /// For other changes to a message, developer interaction is required.
    /// All affected translations will be annotated to indicate that they are outdated.
    /// Our checks will fail as long as such annotations are present.
    /// Developers have several options to address them, depending on the situation.
    /// If the developer is familiar with the language of the translation, they can manually update
    /// the translation as required and removed the annotation.
    /// Otherwise, `cargo xtask fluent resolve-outdated` can be used.
    /// See its description for details.
    Update {
        /// Path to the directory into which the message definitions from the Rust sources have been extracted.
        /// If this is not specified, fish will be compiled with the `fluent-extract` feature to
        /// obtain the message definitions.
        #[arg(long)]
        extraction_dir: Option<PathBuf>,
    },
}

#[derive(Args)]
pub struct RenameArgs {
    /// The message ID.
    /// Use 'old=new', where 'old' is the old message ID as currently present in the file,
    /// and 'new' the name it should be renamed to.
    /// If the ID should not change, use 'old'.
    id: String,
    /// The variables to rename for the specified ID.
    /// Use 'old=new', where 'old' is the old variable name as currently present in the file,
    /// and 'new' the name it should be renamed to.
    vars: Vec<String>,
}

#[derive(Subcommand)]
pub enum ResolveOutdatedCommand {
    /// Delete the message.
    /// Use this when the meaning of the message changed significantly.
    Delete(ResolveOutdatedArgs),
    /// Remove the annotation.
    /// Use this when the translation is already up-to-date.
    Ignore(ResolveOutdatedArgs),
    /// Keep the message as is and replace the OUTDATED annotation with a NEEDS-REVIEW annotation to
    /// indicate that translators should have a look at it.
    /// Use this for minor changes to the English message, which do not change its meaning much.
    NeedsReview(ResolveOutdatedArgs),
    /// Show unresolved messages.
    ShowUnresolved {
        /// Show outdated translations for this message, instead of all messages with outdated
        /// translations.
        /// Can be specified multiple times.
        #[arg(long, value_name = "Message ID")]
        id: Vec<String>,
        /// Consider translations for this language instead of all languages.
        /// Can be specified multiple times.
        /// The value must either be a path to an FTL file or the name of a language as it appears
        /// in one of our FTL file names.
        /// The `.ftl` suffix is optional when specifying the language name.
        #[arg(long)]
        language: Vec<PathBuf>,
        /// Print the outdated message definitions.
        #[arg(long)]
        definitions: bool,
    },
}

#[derive(Args)]
pub struct ResolveOutdatedArgs {
    /// A Fluent message ID of a message annotated as outdated.
    message_id: String,
    /// Paths to any number of FTL files. If none are specified, all FTL files are considered.
    files: Vec<PathBuf>,
}

impl ResolveOutdatedArgs {
    fn get_file_paths(&self) -> Result<Vec<PathBuf>> {
        if self.files.is_empty() {
            non_default_ftl_files()
        } else {
            Ok(self.files.clone())
        }
    }
}

fn non_default_ftl_files() -> Result<Vec<PathBuf>> {
    let all_ftl_files = files_with_extension([ftl_dir()], "ftl")?;
    let mut non_default_files = Vec::with_capacity(all_ftl_files.len() - 1);
    let default_path = default_ftl_file().canonicalize()?;
    for path in all_ftl_files {
        if path.canonicalize()? != default_path {
            non_default_files.push(path);
        }
    }
    Ok(non_default_files)
}

pub fn fluent(args: FluentCommandArgs) -> Result<()> {
    use FluentCommandArgs::*;
    match args {
        Check { from_source, files } => check(from_source, files),
        Format { files } => format(files),
        Rename(rename_args) => rename(rename_args),
        ResolveOutdated(command) => resolve_outdated(command),
        ShowMissing { files } => show_missing(files),
        Update { extraction_dir } => update(extraction_dir),
    }
}

fn check(from_source: Option<Option<PathBuf>>, mut files: Vec<PathBuf>) -> Result<()> {
    if let Some(source) = from_source {
        let generated_resource = generate_default_resource(source)?;
        let mut diff_process = Command::new("diff")
            .arg("-u")
            .arg(default_ftl_file().as_ref())
            .arg("-")
            .stdin(Stdio::piped())
            .spawn()
            .context("Failed to run diff")?;
        diff_process
            .stdin
            .take()
            .unwrap()
            .write_all(serialize_resource(&generated_resource).as_bytes())
            .context("Failed to write to stdin of diff process")?;
        let diff_status = diff_process
            .wait()
            .context("Failed to wait for diff child.")?;
        if !diff_status.success() {
            bail!(
                "{:?} is not up to date.\n\
                Run `cargo xtask fluent update` to resolve this.",
                default_ftl_file()
            );
        }
    }
    if files.is_empty() {
        files = non_default_ftl_files()?;
    }
    check_all_resource_files(default_ftl_file(), &files)
}

fn format(mut files: Vec<PathBuf>) -> Result<()> {
    if files == [PathBuf::from("-")] {
        let mut input = String::new();
        std::io::stdin().read_to_string(&mut input).unwrap();
        let formatted_text = format_text(&input).with_context(|| {
            print!("{input}");
            "Formatting input failed"
        })?;
        print!("{formatted_text}");
        return Ok(());
    }

    if files.is_empty() {
        files = files_with_extension([ftl_dir()], "ftl")?;
    }
    let errors = files
        .iter()
        .filter_map(|path| {
            fluent_ftl_tools::format::format_path(path, FormattingMode::Rewrite).err()
        })
        .collect::<Vec<anyhow::Error>>();
    if !errors.is_empty() {
        let mut error_message = String::from("Found these errors:\n");
        for e in errors {
            error_message.push_str(&format!("{e}\n"));
        }
        bail!("{error_message}");
    }
    Ok(())
}

fn rename(args: RenameArgs) -> Result<()> {
    let old_id;
    let new_id;
    match args.id.split_once('=') {
        Some((old, new)) => {
            old_id = old.into();
            new_id = Some(new.into());
        }
        None => {
            old_id = args.id;
            new_id = None;
        }
    }
    let mut variable_update = HashMap::new();
    for arg in args.vars {
        match arg.split_once('=') {
            Some((old, new)) => {
                variable_update.insert(old.into(), new.into());
            }
            None => {
                bail!("Argument '{arg}' must use the format 'old=new'")
            }
        }
    }
    let files = files_with_extension([ftl_dir()], "ftl")?;
    let resources = rename_in_all_files(&files, &old_id, &new_id, &variable_update)
        .map_err(|e| anyhow!("Failed to perform renaming:\n{e}"))?;
    serialize_resources_to_files(&resources).map_err(|e| anyhow!("Failed to update files:\n{e}"))
}

fn show_missing(mut files: Vec<PathBuf>) -> Result<()> {
    if files.is_empty() {
        files = files_with_extension([ftl_dir()], "ftl")?;
    }
    match find_missing_message_ids_in_files(default_ftl_file(), &files) {
        Ok(None) => {
            println!("No missing messages.");
        }
        Ok(Some(missing_message)) => {
            println!("{missing_message}");
        }
        Err(e) => {
            bail!("Error:\n{e}");
        }
    }
    Ok(())
}

fn resolve_outdated(command: ResolveOutdatedCommand) -> Result<()> {
    match command {
        ResolveOutdatedCommand::Delete(resolve_outdated_args) => {
            let paths = resolve_outdated_args.get_file_paths()?;
            delete_message_from_paths(&resolve_outdated_args.message_id, &paths)
        }
        ResolveOutdatedCommand::Ignore(resolve_outdated_args) => {
            let files = resolve_outdated_args.get_file_paths()?;
            remove_annotation_from_messages(
                Annotation::Outdated,
                &HashSet::from_iter([resolve_outdated_args.message_id]),
                &files,
            )
        }
        ResolveOutdatedCommand::NeedsReview(resolve_outdated_args) => {
            let files = resolve_outdated_args.get_file_paths()?;
            modify_messages(
                |message| {
                    message.add_annotation(Annotation::NeedsReview)?;
                    message.remove_annotation(Annotation::Outdated)
                },
                &HashSet::from_iter([resolve_outdated_args.message_id]),
                &files,
            )
        }
        ResolveOutdatedCommand::ShowUnresolved {
            id: message_ids,
            language: languages,
            definitions,
        } => {
            let languages = if languages.is_empty() {
                non_default_ftl_files()?
            } else {
                let mut language_paths = Vec::with_capacity(languages.len());
                let ftl_dir = ftl_dir().canonicalize()?;
                for lang in languages {
                    if std::fs::exists(&lang)? {
                        language_paths.push(lang);
                        continue;
                    }
                    if let Some(lang_name) = lang.to_str() {
                        let lang_name = lang_name.strip_suffix(".ftl").unwrap_or(lang_name);
                        let file_path = ftl_dir.join(lang_name).with_extension("ftl");
                        if std::fs::exists(&file_path)? {
                            language_paths.push(file_path);
                            continue;
                        }
                    }
                    bail!(
                        "Language {lang:?} is invalid. It must be a path to an FTL file or the name of a language as present in a file name of one of our FTL files."
                    )
                }
                language_paths
            };
            let id_set: HashSet<&String> = HashSet::from_iter(&message_ids);
            for lang in &languages {
                println!("{lang:?}:");
                let mut resource =
                    filter_resource_messages(parse_as_syntax_resource(lang)?, |message| {
                        message.has_annotation(Annotation::Outdated)
                    })?;
                if !id_set.is_empty() {
                    resource = filter_resource_messages(resource, |message| {
                        Ok(id_set.contains(&message.id.name))
                    })?;
                }
                for entry in &resource.body {
                    let Entry::Message(message) = entry else {
                        continue;
                    };
                    if definitions {
                        let mut serializer =
                            Serializer::new(fluent_syntax::serializer::Options { with_junk: true });
                        serializer.serialize_message(message);
                        let serialized_message = serializer.into_serialized_text();
                        println!("{serialized_message}");
                    } else {
                        println!("{}", message.id.name);
                    }
                }
                println!();
            }
            Ok(())
        }
    }
}

fn update(extraction_dir: Option<PathBuf>) -> Result<()> {
    let old_default_resource = parse_as_syntax_resource(default_ftl_file())
        .context("Failed to parse existing default FTL file")?;
    let generated_resource = generate_default_resource(extraction_dir)
        .context("Failed to generate new default FTL file")?;
    let ids_of_outdated_messages =
        get_message_ids_with_different_value(&old_default_resource, &generated_resource);
    let non_default_ftl_paths = non_default_ftl_files()?;
    remove_inconsistent_translations(
        generated_resource.all_message_vars()?,
        &non_default_ftl_paths,
    )?;
    add_annotation_to_messages(
        Annotation::Outdated,
        &ids_of_outdated_messages,
        &non_default_ftl_paths,
    )?;
    serialize_resource_to_file(&generated_resource, default_ftl_file())
        .context("Failed to update FTL file for default language.")?;
    Ok(())
}

fn generate_default_resource(extraction_dir: Option<PathBuf>) -> Result<Resource<String>> {
    fn concat_unique_file_content<P: AsRef<Path>>(dir: P) -> Result<String> {
        let mut unique_file_contents = HashSet::new();
        for dir_entry in std::fs::read_dir(&dir)
            .with_context(|| format!("Failed to read from directory {:?}", dir.as_ref()))?
        {
            let dir_entry = dir_entry
                .with_context(|| format!("Error traversing directory {:?}", dir.as_ref()))?;
            if dir_entry
                .file_type()
                .with_context(|| format!("Could not get file type for {:?}", dir_entry.path()))?
                .is_file()
            {
                unique_file_contents.insert(
                    std::fs::read_to_string(dir_entry.path()).with_context(|| {
                        format!("Could not read from file {:?}", dir_entry.path())
                    })?,
                );
            }
        }
        let mut concatenated_content = String::new();
        unique_file_contents
            .iter()
            .for_each(|file_content| concatenated_content.push_str(file_content));
        Ok(concatenated_content)
    }

    let id_file_content = match extraction_dir {
        Some(dir) => concat_unique_file_content(dir).unwrap(),
        None => {
            let temp_dir = fish_tempfile::new_dir().unwrap();
            Command::new(env!("CARGO"))
                .args([
                    "check",
                    "--workspace",
                    "--all-targets",
                    "--features=fluent-extract",
                ])
                .env("FISH_FLUENT_EXTRACTION_DIR", temp_dir.path().as_os_str())
                .status()
                .map_err(|e| format!("Failed to extract Fluent IDs: {e}"))
                .unwrap();
            concat_unique_file_content(temp_dir.path()).unwrap()
        }
    };
    let resource = parse_str_as_syntax_resource(&id_file_content).map_err(|e| anyhow!("{e}"))?;
    format_resource(resource).map_err(|e| anyhow!("{e}"))
}
