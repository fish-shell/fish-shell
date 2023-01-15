/// Support for tests which need to cross the FFI.
/// Because the C++ is not compiled by `cargo test` and there is no natural way to
/// do it, use the following facilities for tests which need to use C++ types.
/// This uses the inventory crate to build a custom-test harness
/// as described at https://www.infinyon.com/blog/2021/04/rust-custom-test-harness/
/// See smoke.rs add_test for an example of how to use this.

#[cfg(all(feature = "fish-ffi-tests", not(test)))]
mod ffi_tests_impl {
    use inventory;

    /// A test which needs to cross the FFI.
    #[derive(Debug)]
    pub struct FFITest {
        pub name: &'static str,
        pub func: fn(),
    }

    /// Add a new test.
    /// Example usage:
    /// ```
    ///   add_test!("test_name", || {
    ///       assert!(1 + 2 == 3);
    ///   });
    /// ```
    macro_rules! add_test {
        ($name:literal, $func:expr) => {
            inventory::submit!(crate::ffi_tests::FFITest {
                name: $name,
                func: $func,
            });
        };
    }
    pub(crate) use add_test;

    inventory::collect!(crate::ffi_tests::FFITest);

    /// Runs all ffi tests.
    pub fn run_ffi_tests() {
        for test in inventory::iter::<crate::ffi_tests::FFITest> {
            println!("Running ffi test {}", test.name);
            (test.func)();
        }
    }
}

#[cfg(not(all(feature = "fish-ffi-tests", not(test))))]
mod ffi_tests_impl {
    macro_rules! add_test {
        ($name:literal, $func:expr) => {};
    }
    pub(crate) use add_test;
    pub fn run_ffi_tests() {}
}

pub(crate) use ffi_tests_impl::*;

#[cxx::bridge(namespace = rust)]
mod ffi_tests {
    extern "Rust" {
        fn run_ffi_tests();
    }
}
