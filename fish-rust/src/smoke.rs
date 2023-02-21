#[cxx::bridge(namespace = rust)]
mod ffi {
    extern "Rust" {
        fn add(left: usize, right: usize) -> usize;
    }
}

pub fn add(left: usize, right: usize) -> usize {
    left + right
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        let result = add(2, 2);
        assert_eq!(result, 4);
    }
}

use crate::ffi_tests::add_test;
add_test!("test_add", || {
    assert_eq!(add(2, 3), 5);
});
