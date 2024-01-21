use std::ffi::CStr;
use std::mem::MaybeUninit;

/// The buffer type we use for formatting an int as a C string.
/// 24 bytes is enough to format any i64 or u64.
type StackBuffer = MaybeUninit<[u8; 24]>;

fn format_int(buff: &mut StackBuffer, mut val: u64, neg: bool) -> &[u8] {
    // Note while panic is unsafe, we freely allow bounds checking and checked
    // arithmetic here, as StackBuffer is sufficient for any i64 or u64.
    if val == 0 {
        return b"0";
    }
    let buff: &mut [u8; 24] = buff.write([0; 24]);
    // Start at the end of the buffer to avoid reversing it.
    // cursor is the location of the last byte written.
    // The cursor > 1 check is superfluous, it simply avoids some bounds checks
    // in the generated code.
    let mut cursor = buff.len();
    while val != 0 && cursor > 1 {
        let digit = val % 10;
        val /= 10;
        buff[cursor - 1] = b'0' + digit as u8;
        cursor -= 1;
    }
    if neg {
        buff[cursor - 1] = b'-';
        cursor -= 1;
    }
    &buff[cursor..]
}

/// A trait like FloggableDisplay but for async-signal-safe types.
/// Bytes are assumed UTF-8 - only pass ASCII string literals here.
pub trait FloggableDisplayAsyncSafe {
    /// Return a slice of u8, optionally relying on the storage for formatting.
    /// This function is called from an async-signal-safe context. It must not panic, allocate, etc.
    /// This is inlined to elide the creation of StackBuffer if it is unused.
    fn to_flog_str_async_safe<'a>(&'a self, storage: &'a mut StackBuffer) -> &'a [u8];
}

impl FloggableDisplayAsyncSafe for &str {
    fn to_flog_str_async_safe(&self, _storage: &mut StackBuffer) -> &[u8] {
        self.as_bytes()
    }
}

impl FloggableDisplayAsyncSafe for &[u8] {
    fn to_flog_str_async_safe(&self, _storage: &mut StackBuffer) -> &[u8] {
        self
    }
}

impl FloggableDisplayAsyncSafe for i64 {
    fn to_flog_str_async_safe<'a>(&self, storage: &'a mut StackBuffer) -> &'a [u8] {
        format_int(storage, self.unsigned_abs(), (*self) < 0)
    }
}

impl FloggableDisplayAsyncSafe for i32 {
    fn to_flog_str_async_safe<'a>(&'a self, storage: &'a mut StackBuffer) -> &'a [u8] {
        format_int(storage, self.unsigned_abs().into(), (*self) < 0)
    }
}

impl FloggableDisplayAsyncSafe for usize {
    fn to_flog_str_async_safe<'a>(&'a self, storage: &'a mut StackBuffer) -> &'a [u8] {
        format_int(storage, (*self) as u64, false)
    }
}

impl FloggableDisplayAsyncSafe for &CStr {
    fn to_flog_str_async_safe<'a>(&'a self, _storage: &'a mut StackBuffer) -> &'a [u8] {
        self.to_bytes()
    }
}

/// Variant of flog_impl which is async-signal safe.
pub fn flog_impl_async_safe(fd: i32, s: impl FloggableDisplayAsyncSafe) {
    if fd < 0 {
        return;
    }
    let mut storage = StackBuffer::uninit();
    let bytes: &[u8] = s.to_flog_str_async_safe(&mut storage);
    // Note we deliberately do not retry on signals, etc.
    // This is used to report error messages after fork() in the child process.
    unsafe {
        let _ = libc::write(fd, bytes.as_ptr() as *const libc::c_void, bytes.len());
    }
}

/// Variant of FLOG which is async-safe to use after fork().
/// This does not allocate or take locks. Only str and nul-terminated C-strings are supported.
/// The arguments are NOT space-separated. Embed real spaces in your literals.
macro_rules! FLOG_SAFE {
    ($category:ident, $($elem:expr),+ $(,)*) => {
        if crate::flog::categories::$category
            .enabled
            .load(std::sync::atomic::Ordering::Relaxed)
        {
            #[allow(unused_imports)]
            use crate::fork_exec::flog_safe::{flog_impl_async_safe, FloggableDisplayAsyncSafe};
            let fd = crate::flog::get_flog_file_fd();
            flog_impl_async_safe(fd, stringify!($category));
            flog_impl_async_safe(fd, ": ");
            $(
                flog_impl_async_safe(fd, $elem);
            )+
            // We always append a newline.
            flog_impl_async_safe(fd, "\n");
        }
    };
}

pub(crate) use FLOG_SAFE;

#[cfg(test)]
mod tests {

    use super::*;

    fn test_1_int<T: FloggableDisplayAsyncSafe + std::fmt::Display>(val: T) {
        let mut storage = StackBuffer::uninit();
        let bytes: &[u8] = val.to_flog_str_async_safe(&mut storage);
        assert_eq!(bytes, val.to_string().as_bytes());
    }

    #[test]
    fn test_int_to_flog_str() {
        for x in -1024..=1024 {
            test_1_int(x);
            test_1_int(x as i64);
            test_1_int(x as usize);
        }
        test_1_int(i32::MIN);
        test_1_int(i32::MAX);
        test_1_int(i64::MIN);
        test_1_int(i64::MAX);
        test_1_int(usize::MIN);
        test_1_int(usize::MAX);
    }

    #[test]
    fn test_str_to_flog_cstr() {
        use std::ffi::CStr;
        let mut buffer = StackBuffer::uninit();

        let str = CStr::from_bytes_with_nul(b"hello\0").unwrap();
        let result = str.to_flog_str_async_safe(&mut buffer);
        assert_eq!(result, b"hello");

        let str = CStr::from_bytes_with_nul(b"\0").unwrap();
        let result = str.to_flog_str_async_safe(&mut buffer);
        assert_eq!(result, b"");

        let str = "hello";
        let result = str.to_flog_str_async_safe(&mut buffer);
        assert_eq!(result, b"hello");

        let str = "";
        let result = str.to_flog_str_async_safe(&mut buffer);
        assert_eq!(result, b"");
    }
}
