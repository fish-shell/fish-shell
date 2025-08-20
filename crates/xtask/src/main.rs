use std::process::Command;

fn main() {
    // Args passed to xtasks, not including the binary name which gets set automatically.
    let args = std::env::args().skip(1).collect::<Vec<_>>();
    if args.is_empty() {
        panic!("Called xtask without arguments. Doing nothing.");
    }

    let version = fish_version::get_version();
    std::env::set_var("FISH_BUILD_VERSION", version);

    let command = &args[0];
    let command_args = &args[1..];
    match command.as_str() {
        "cargo" | "c" => match Command::new(env!("CARGO")).args(command_args).status() {
            Ok(exit_status) => {
                if !exit_status.success() {
                    panic!("Cargo did not run successfully.");
                }
            }
            Err(err) => {
                panic!("Failed to run cargo: {err}");
            }
        },
        other => {
            panic!("Unknown xtask: {other}");
        }
    }
}
