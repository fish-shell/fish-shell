#[cfg(test)]
mod tests {
    use fish_fluent::{fluent_ids, localize};

    #[test]
    fn without_args() {
        assert_eq!(localize!("test"), "This is a test");
    }

    #[test]
    fn with_args() {
        fluent_ids! {
            ID "test-with-args"
        }
        assert_eq!(
            localize!(ID, first = 1, second = "two"),
            "Two arguments: 1, two"
        );

        let mut args = fluent::FluentArgs::new();
        args.set("first", 1);
        args.set("second", "two");
        assert_eq!(localize!(ID, &args), "Two arguments: 1, two");
    }
}
