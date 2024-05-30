use widestring::utf32str;

use crate::{builtins::{math, prelude::Parser}, io::{IoChain, IoStreams, OutputStream, StringOutputStream}, threads};

#[test]
fn test_round() {
    threads::init();
    let parser = &Parser::principal_parser();
    let mut out = OutputStream::String(StringOutputStream::new());
    let mut err = OutputStream::String(StringOutputStream::new());
    let io_chain = IoChain::new();
    let mut stream = IoStreams::new(&mut out, &mut err, &io_chain);

    let mut argv = [utf32str!("math"), utf32str!("--round=0"), utf32str!("22 / 5 - 5")];
    let _res = math::math(parser, &mut stream, &mut argv);
    assert!(stream.out.contents() == "-1\n");
}

// fn test_unit()