use crate::complete::CompleteFlags;
use crate::reader::{combine_command_and_autosuggestion, completion_apply_to_command_line};
use crate::wchar::prelude::*;

#[test]
fn test_autosuggestion_combining() {
    assert_eq!(
        combine_command_and_autosuggestion(L!("alpha"), L!("alphabeta")),
        L!("alphabeta")
    );

    // When the last token contains no capital letters, we use the case of the autosuggestion.
    assert_eq!(
        combine_command_and_autosuggestion(L!("alpha"), L!("ALPHABETA")),
        L!("ALPHABETA")
    );

    // When the last token contains capital letters, we use its case.
    assert_eq!(
        combine_command_and_autosuggestion(L!("alPha"), L!("alphabeTa")),
        L!("alPhabeTa")
    );

    // If autosuggestion is not longer than input, use the input's case.
    assert_eq!(
        combine_command_and_autosuggestion(L!("alpha"), L!("ALPHAA")),
        L!("ALPHAA")
    );
    assert_eq!(
        combine_command_and_autosuggestion(L!("alpha"), L!("ALPHA")),
        L!("alpha")
    );
}

#[test]
fn test_completion_insertions() {
    macro_rules! validate {
        (
            $line:expr, $completion:expr,
            $flags:expr, $append_only:expr,
            $expected:expr
        ) => {
            // line is given with a caret, which we use to represent the cursor position. Find it.
            let mut line = L!($line).to_owned();
            let completion = L!($completion);
            let mut expected = L!($expected).to_owned();
            let in_cursor_pos = line.find(L!("^")).unwrap();
            line.remove(in_cursor_pos);

            let out_cursor_pos = expected.find(L!("^")).unwrap();
            expected.remove(out_cursor_pos);

            let mut cursor_pos = in_cursor_pos;
            let result = completion_apply_to_command_line(
                completion,
                $flags,
                &line,
                &mut cursor_pos,
                $append_only,
                /*is_unique=*/ false,
            );
            assert_eq!(result, expected);
            assert_eq!(cursor_pos, out_cursor_pos);
        };
    }

    validate!("foo^", "bar", CompleteFlags::default(), false, "foobar ^");
    // An unambiguous completion of a token that is already trailed by a space character.
    // After completing, the cursor moves on to the next token, suggesting to the user that the
    // current token is finished.
    validate!(
        "foo^ baz",
        "bar",
        CompleteFlags::default(),
        false,
        "foobar ^baz"
    );
    validate!(
        "'foo^",
        "bar",
        CompleteFlags::default(),
        false,
        "'foobar' ^"
    );
    validate!(
        "'foo'^",
        "bar",
        CompleteFlags::default(),
        false,
        "'foobar' ^"
    );
    validate!(
        "'foo\\'^",
        "bar",
        CompleteFlags::default(),
        false,
        "'foo\\'bar' ^"
    );
    validate!(
        "foo\\'^",
        "bar",
        CompleteFlags::default(),
        false,
        "foo\\'bar ^"
    );

    // Test append only.
    validate!("foo^", "bar", CompleteFlags::default(), true, "foobar ^");
    validate!(
        "foo^ baz",
        "bar",
        CompleteFlags::default(),
        true,
        "foobar ^baz"
    );
    validate!("'foo^", "bar", CompleteFlags::default(), true, "'foobar' ^");
    validate!(
        "'foo'^",
        "bar",
        CompleteFlags::default(),
        true,
        "'foo'bar ^"
    );
    validate!(
        "'foo\\'^",
        "bar",
        CompleteFlags::default(),
        true,
        "'foo\\'bar' ^"
    );
    validate!(
        "foo\\'^",
        "bar",
        CompleteFlags::default(),
        true,
        "foo\\'bar ^"
    );

    validate!("foo^", "bar", CompleteFlags::NO_SPACE, false, "foobar^");
    validate!("'foo^", "bar", CompleteFlags::NO_SPACE, false, "'foobar^");
    validate!("'foo'^", "bar", CompleteFlags::NO_SPACE, false, "'foobar'^");
    validate!(
        "'foo\\'^",
        "bar",
        CompleteFlags::NO_SPACE,
        false,
        "'foo\\'bar^"
    );
    validate!(
        "foo\\'^",
        "bar",
        CompleteFlags::NO_SPACE,
        false,
        "foo\\'bar^"
    );

    validate!("foo^", "bar", CompleteFlags::REPLACES_TOKEN, false, "bar ^");
    validate!(
        "'foo^",
        "bar",
        CompleteFlags::REPLACES_TOKEN,
        false,
        "bar ^"
    );

    // See #6130
    validate!(": (:^ ''", "", CompleteFlags::default(), false, ": (: ^''");
}
