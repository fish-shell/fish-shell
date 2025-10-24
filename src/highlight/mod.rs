pub mod file_tester;
#[allow(clippy::module_inception)]
mod highlight;
pub use file_tester::is_potential_path;
pub use highlight::*;
