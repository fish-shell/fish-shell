use widestring_suffix::widestrs;

mod wchar {
    macro_rules! L {
        ($string:expr) => {
            42
        };
    }

    pub(crate) use L;
}

#[widestrs]
mod stuff {
    pub fn test1() {
        let s = "abc"L;
        assert_eq!(s, 42);
    }
}

#[test]
fn test_widestring() {
    stuff::test1();
}
