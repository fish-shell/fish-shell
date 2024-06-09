// Re-export sprintf macro.
pub use fish_printf::sprintf;

#[cfg(test)]
mod tests {
    use super::*;

    // Test basic sprintf with both literals and wide strings.
    #[test]
    fn test_sprintf() {
        assert_eq!(sprintf!("Hello, %s!", "world"), "Hello, world!");
        assert_eq!(sprintf!("Hello, %ls!", "world"), "Hello, world!");
        assert_eq!(sprintf!("Hello, %ls!", "world"), "Hello, world!");
    }
}
