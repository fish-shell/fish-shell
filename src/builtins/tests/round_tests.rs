use widestring::utf32str;

use super::super::prelude::*;

use crate::{
    builtins::{math, prelude::Parser},
    io::{IoChain, IoStreams, OutputStream, StringOutputStream},
    threads,
};

fn test_math(
    argv: &mut [&wstr],
    expected_ans: &str,
    parser: &&Parser,
    stream: &mut IoStreams,
    ans_lst: &mut String,
) {
    let _res = math::math(parser, stream, argv);
    ans_lst.push_str(expected_ans);
    ans_lst.push_str("\n");
    assert!(stream.out.contents() == ans_lst);
}

#[test]
fn test_round() {
    threads::init();
    let parser = &Parser::principal_parser();
    let mut out = OutputStream::String(StringOutputStream::new());
    let mut err = OutputStream::String(StringOutputStream::new());
    let io_chain = IoChain::new();
    let mut stream = IoStreams::new(&mut out, &mut err, &io_chain);
    let mut ans_lst = String::new();

    let mut argv = [
        utf32str!("math"),
        utf32str!("-s"),
        utf32str!("0"),
        utf32str!("--scale-mode=trunc"),
        utf32str!("22 / 5 - 5"),
    ];
    test_math(&mut argv, "-0", parser, &mut stream, &mut ans_lst);

    let mut argv = [
        utf32str!("math"),
        utf32str!("--scale=0"),
        utf32str!("-m"),
        utf32str!("trunc"),
        utf32str!("22 / 5 - 5"),
    ];
    test_math(&mut argv, "-0", parser, &mut stream, &mut ans_lst);

    let mut argv = [
        utf32str!("math"),
        utf32str!("-s"),
        utf32str!("0"),
        utf32str!("--scale-mode=floor"),
        utf32str!("22 / 5 - 5"),
    ];
    test_math(&mut argv, "-1", parser, &mut stream, &mut ans_lst);

    let mut argv = [
        utf32str!("math"),
        utf32str!("-s"),
        utf32str!("0"),
        utf32str!("--scale-mode=round"),
        utf32str!("22 / 5 - 5"),
    ];
    test_math(&mut argv, "-1", parser, &mut stream, &mut ans_lst);

    let mut argv = [
        utf32str!("math"),
        utf32str!("-s"),
        utf32str!("0"),
        utf32str!("--scale-mode=ceil"),
        utf32str!("22 / 5 - 5"),
    ];
    test_math(&mut argv, "-0", parser, &mut stream, &mut ans_lst);
}
