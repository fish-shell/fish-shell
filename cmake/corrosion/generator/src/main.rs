use std::path::PathBuf;

use cargo_metadata::Metadata;
use clap::{App, Arg};

mod subcommands {
    pub mod gen_cmake;
    pub mod print_root;
}

use subcommands::*;

// common options
const MANIFEST_PATH: &str = "manifest-path";
const CARGO_EXECUTABLE: &str = "cargo-executable";
const VERBOSE: &str = "verbose";

pub struct GeneratorSharedArgs {
    pub manifest_path: PathBuf,
    pub cargo_executable: PathBuf,
    pub metadata: Metadata,
    pub verbose: bool,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let matches = App::new("CMake Generator for Cargo")
        .version("0.1")
        .author("Andrew Gaspar <andrew.gaspar@outlook.com>")
        .about("Generates CMake files for Cargo projects")
        .arg(
            Arg::with_name(MANIFEST_PATH)
                .long("manifest-path")
                .value_name("Cargo.toml")
                .help("Specifies the target Cargo project")
                .required(true)
                .takes_value(true),
        )
        .arg(
            Arg::with_name(CARGO_EXECUTABLE)
                .long("cargo")
                .value_name("EXECUTABLE")
                .required(true)
                .help("Path to the cargo executable to use"),
        )
        .arg(
            Arg::with_name(VERBOSE)
                .long("verbose")
                .help("Request verbose output"),
        )
        .subcommand(print_root::subcommand())
        .subcommand(gen_cmake::subcommand())
        .get_matches();

    let mut cmd = cargo_metadata::MetadataCommand::new();

    let manifest_path = matches.value_of(MANIFEST_PATH).unwrap();
    let cargo_executable = matches.value_of(CARGO_EXECUTABLE).unwrap();

    cmd.manifest_path(manifest_path);
    cmd.cargo_path(cargo_executable);

    let metadata = cmd.exec()?;

    let shared_args = GeneratorSharedArgs {
        manifest_path: manifest_path.into(),
        cargo_executable: cargo_executable.into(),
        metadata,
        verbose: matches.is_present(VERBOSE),
    };

    match matches.subcommand() {
        (print_root::PRINT_ROOT, _) => print_root::invoke(&shared_args)?,
        (gen_cmake::GEN_CMAKE, Some(matches)) => gen_cmake::invoke(&shared_args, matches)?,
        _ => unreachable!(),
    };

    // We should never reach this statement
    std::process::exit(1);
}
