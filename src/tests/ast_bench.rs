// Run with cargo +nightly bench --features=benchmark
#[cfg(feature = "benchmark")]
mod bench {
    extern crate test;
    use crate::ast;
    use crate::wchar::prelude::*;
    use test::Bencher;

    // Return a long string suitable for benchmarking.
    fn generate_fish_script() -> WString {
        let mut buff = WString::new();
        let s = &mut buff;

        for i in 0..1000 {
            // command with args and redirections
            sprintf!(=> s,
                "echo arg%d arg%d > out%d.txt 2> err%d.txt\n",
                i, i + 1, i, i
            );

            // simple block
            sprintf!(=> s, "begin\n    echo inside block %d\nend\n", i );

            // conditional
            sprintf!(=> s, "if test %d\n    echo even\nelse\n    echo odd\nend\n", i % 2);

            // loop
            sprintf!(=> s, "for x in a b c\n    echo $x %d\nend\n", i);

            // pipeline
            sprintf!(=> s, "echo foo%d | grep f | wc -l\n", i);
        }

        buff
    }

    #[bench]
    fn bench_ast_construction(b: &mut Bencher) {
        let src = generate_fish_script();
        b.bytes = (src.len() * 4) as u64; // 4 bytes per character
        b.iter(|| {
            let _ast = ast::parse(&src, Default::default(), None);
        });
    }
}
