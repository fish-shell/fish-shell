pub mod shared;

pub mod abbr;
pub mod argparse;
pub mod bg;
pub mod block;
pub mod builtin;
pub mod cd;
pub mod command;
pub mod contains;
pub mod echo;
pub mod emit;
pub mod exit;
pub mod math;
pub mod printf;
pub mod pwd;
pub mod random;
pub mod realpath;
pub mod r#return;
pub mod set_color;
pub mod status;
pub mod test;
pub mod r#type;
pub mod wait;

// Note these tests will NOT run with cfg(test).
mod tests;
