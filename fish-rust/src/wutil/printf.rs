// Re-export sprintf macro.
pub use printf_compat::sprintf;

#[cfg(test)]
mod tests {
    use super::*;
    use crate::wchar::L;

    // Test basic sprintf with both literals and wide strings.
    #[test]
    fn test_sprintf() {
        assert_eq!(sprintf!("Hello, %s!", "world"), "Hello, world!");
        assert_eq!(sprintf!(L!("Hello, %ls!"), "world"), "Hello, world!");
        assert_eq!(sprintf!(L!("Hello, %ls!"), L!("world")), "Hello, world!");
    }
}
