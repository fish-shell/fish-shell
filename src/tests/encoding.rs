use crate::common::{bytes2wcstring, wcs2bytes};
use crate::wchar::prelude::*;

/// Verify correct behavior with embedded nulls.
#[test]
fn test_convert_nulls() {
    let input = L!("AAA\0BBB");
    let out_str = wcs2bytes(input);
    assert_eq!(
        input.chars().collect::<Vec<_>>(),
        std::str::from_utf8(&out_str)
            .unwrap()
            .chars()
            .collect::<Vec<_>>()
    );

    let out_wstr = bytes2wcstring(&out_str);
    assert_eq!(input, &out_wstr);
}

#[cfg(feature = "benchmark")]
mod bench {
    extern crate test;
    use crate::tests::encoding::bytes2wcstring;
    use test::Bencher;

    #[bench]
    fn bench_convert_ascii(b: &mut Bencher) {
        let s: [u8; 128 * 1024] = std::array::from_fn(|i| b'0' + u8::try_from(i % 10).unwrap());
        b.bytes = u64::try_from(s.len()).unwrap();
        b.iter(|| bytes2wcstring(&s));
    }
}
