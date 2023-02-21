use std::{
    fs::{create_dir_all, File},
    io::{stdout, Write},
    path::Path,
    rc::Rc,
};

use clap::{App, Arg, ArgMatches, SubCommand};

mod target;

// Command name
pub const GEN_CMAKE: &str = "gen-cmake";

// Options
const OUT_FILE: &str = "out-file";
const CONFIGURATION_ROOT: &str = "configuration-root";
const PROFILE: &str = "profile";
const CRATES: &str = "crates";

pub fn subcommand() -> App<'static, 'static> {
    SubCommand::with_name(GEN_CMAKE)
        .arg(
            Arg::with_name(CONFIGURATION_ROOT)
                .long("configuration-root")
                .value_name("DIRECTORY")
                .takes_value(true)
                .help(
                    "Specifies a root directory for configuration folders. E.g. Win32 \
                 in VS Generator.",
                ),
        )
        .arg(
            Arg::with_name(CRATES)
                .long("crates")
                .value_name("crates")
                .takes_value(true)
                .multiple(true)
                .require_delimiter(true)
                .help("Specifies which crates of the workspace to import"),
        )
        .arg(
            Arg::with_name(PROFILE)
                .long(PROFILE)
                .value_name("PROFILE")
                .required(false)
                .help("Custom cargo profile to select."),
        )
        .arg(
            Arg::with_name(OUT_FILE)
                .short("o")
                .long("out-file")
                .value_name("FILE")
                .help("Output CMake file name. Defaults to stdout."),
        )
}

pub fn invoke(
    args: &crate::GeneratorSharedArgs,
    matches: &ArgMatches,
) -> Result<(), Box<dyn std::error::Error>> {

    let mut out_file: Box<dyn Write> = if let Some(path) = matches.value_of(OUT_FILE) {
        let path = Path::new(path);
        if let Some(parent) = path.parent() {
            create_dir_all(parent).expect("Failed to create directory!");
        }
        let file = File::create(path).expect("Unable to open out-file!");
        Box::new(file)
    } else {
        Box::new(stdout())
    };

    writeln!(
        out_file,
        "\
cmake_minimum_required(VERSION 3.15)
"
    )?;

    let crates = matches
        .values_of(CRATES)
        .map_or(Vec::new(), |c| c.collect());
    let workspace_manifest_path = Rc::new(args.manifest_path.clone());
    let targets: Vec<_> = args
        .metadata
        .packages
        .iter()
        .filter(|p| {
            args.metadata.workspace_members.contains(&p.id)
                && (crates.is_empty() || crates.contains(&p.name.as_str()))
        })
        .cloned()
        .map(Rc::new)
        .flat_map(|package| {
            let package2 = package.clone();
            let ws_manifest_path = workspace_manifest_path.clone();
            package
                .targets
                .clone()
                .into_iter()
                .filter_map(move |t| target::CargoTarget::from_metadata(package2.clone(), t, ws_manifest_path.clone()))
        })
        .collect();

    let cargo_profile = matches.value_of(PROFILE);

    for target in &targets {
        target
            .emit_cmake_target(
                &mut out_file,
                cargo_profile,
            )
            .unwrap();
    }

    writeln!(out_file)?;

    std::process::exit(0);
}
