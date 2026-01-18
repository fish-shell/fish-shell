use super::printf_impl::Error;
use std::result::Result;
#[cfg(feature = "widestring")]
use widestring::{Utf32Str as wstr, Utf32String as WString};

/// Printf argument types.
/// Note no implementation of `ToArg` constructs the owned variants (String and WString);
/// callers can do so explicitly.
#[derive(Debug, PartialEq)]
pub enum Arg<'a> {
    Str(&'a str),
    #[cfg(feature = "widestring")]
    WStr(&'a wstr),
    String(String),
    #[cfg(feature = "widestring")]
    WString(WString),
    UInt(u64),
    SInt(i64),
    Float(f64),
    USizeRef(&'a mut usize), // for use with %n
}

impl<'a> Arg<'a> {
    pub fn set_count(&mut self, count: usize) -> Result<(), Error> {
        match self {
            Arg::USizeRef(p) => **p = count,
            _ => return Err(Error::BadArgType),
        }
        Ok(())
    }

    // Convert this to a narrow string, using the provided storage if necessary.
    // In practice 'storage' is only used if the widestring feature is enabled.
    #[allow(unused_variables, clippy::ptr_arg)]
    pub fn as_str<'s>(&'s self, storage: &'s mut String) -> Result<&'s str, Error>
    where
        'a: 's,
    {
        match self {
            Arg::Str(s) => Ok(s),
            Arg::String(s) => Ok(s),
            #[cfg(feature = "widestring")]
            Arg::WStr(s) => {
                storage.clear();
                storage.extend(s.chars());
                Ok(storage)
            }
            #[cfg(feature = "widestring")]
            Arg::WString(s) => {
                storage.clear();
                storage.extend(s.chars());
                Ok(storage)
            }
            _ => Err(Error::BadArgType),
        }
    }

    // Return this value as an unsigned integer. Negative signed values will report overflow.
    pub fn as_uint(&self) -> Result<u64, Error> {
        match *self {
            Arg::UInt(u) => Ok(u),
            Arg::SInt(i) => i.try_into().map_err(|_| Error::Overflow),
            _ => Err(Error::BadArgType),
        }
    }

    // Return this value as a signed integer. Unsigned values > i64::MAX will report overflow.
    pub fn as_sint(&self) -> Result<i64, Error> {
        match *self {
            Arg::UInt(u) => u.try_into().map_err(|_| Error::Overflow),
            Arg::SInt(i) => Ok(i),
            _ => Err(Error::BadArgType),
        }
    }

    /// Unwraps [`Arg::UInt`] to [`u64`].
    /// Unwraps [`Arg::SInt`] and casts the [`i64`] to [`u64`].
    /// Calling this on other variants of `[Arg]` is an error.
    pub fn as_wrapping_sint(&self) -> Result<u64, Error> {
        match *self {
            Arg::UInt(u) => Ok(u),
            Arg::SInt(i) => Ok(i as u64),
            _ => Err(Error::BadArgType),
        }
    }

    // Note we allow passing ints as floats, even allowing precision loss.
    pub fn as_float(&self) -> Result<f64, Error> {
        #[allow(clippy::cast_precision_loss)]
        match *self {
            Arg::Float(f) => Ok(f),
            Arg::UInt(u) => Ok(u as f64),
            Arg::SInt(i) => Ok(i as f64),
            _ => Err(Error::BadArgType),
        }
    }

    pub fn as_char(&self) -> Result<char, Error> {
        let v: u32 = self.as_uint()?.try_into().map_err(|_| Error::Overflow)?;
        v.try_into().map_err(|_| Error::Overflow)
    }
}

/// Conversion from a raw value to a printf argument.
pub trait ToArg<'a> {
    fn to_arg(self) -> Arg<'a>;
}

impl<'a> ToArg<'a> for &'a str {
    fn to_arg(self) -> Arg<'a> {
        Arg::Str(self)
    }
}

impl<'a> ToArg<'a> for &'a String {
    fn to_arg(self) -> Arg<'a> {
        Arg::Str(self)
    }
}

#[cfg(feature = "widestring")]
impl<'a> ToArg<'a> for &'a wstr {
    fn to_arg(self) -> Arg<'a> {
        Arg::WStr(self)
    }
}

#[cfg(feature = "widestring")]
impl<'a> ToArg<'a> for &'a WString {
    fn to_arg(self) -> Arg<'a> {
        Arg::WStr(self)
    }
}

impl<'a> ToArg<'a> for &'a std::io::Error {
    fn to_arg(self) -> Arg<'a> {
        Arg::String(self.to_string())
    }
}

impl<'a> ToArg<'a> for f32 {
    fn to_arg(self) -> Arg<'a> {
        Arg::Float(self.into())
    }
}

impl<'a> ToArg<'a> for f64 {
    fn to_arg(self) -> Arg<'a> {
        Arg::Float(self)
    }
}

impl<'a> ToArg<'a> for char {
    fn to_arg(self) -> Arg<'a> {
        Arg::UInt((self as u32).into())
    }
}

impl<'a> ToArg<'a> for &'a mut usize {
    fn to_arg(self) -> Arg<'a> {
        Arg::USizeRef(self)
    }
}

impl<'a, T> ToArg<'a> for &'a *const T {
    fn to_arg(self) -> Arg<'a> {
        Arg::UInt((*self) as usize as u64)
    }
}

/// All signed types.
macro_rules! impl_to_arg {
    ($($t:ty),*) => {
        $(
            impl<'a> ToArg<'a> for $t {
                fn to_arg(self) -> Arg<'a> {
                    Arg::SInt(self as i64)
                }
            }
        )*
    };
}
impl_to_arg!(i8, i16, i32, i64, isize);

/// All unsigned types.
macro_rules! impl_to_arg_u {
    ($($t:ty),*) => {
        $(
            impl<'a> ToArg<'a> for $t {
                fn to_arg(self) -> Arg<'a> {
                    Arg::UInt(self as u64)
                }
            }
        )*
    };
}
impl_to_arg_u!(u8, u16, u32, u64, usize);

#[cfg(test)]
mod tests {
    use super::*;
    use assert_matches::assert_matches;
    #[cfg(feature = "widestring")]
    use widestring::utf32str;

    #[test]
    fn test_to_arg() {
        assert_matches!("test".to_arg(), Arg::Str("test"));
        assert_matches!(String::from("test").to_arg(), Arg::Str(_));
        #[cfg(feature = "widestring")]
        assert_matches!(utf32str!("test").to_arg(), Arg::WStr(_));
        #[cfg(feature = "widestring")]
        assert_matches!(WString::from("test").to_arg(), Arg::WStr(_));
        assert_matches!(42f32.to_arg(), Arg::Float(_));
        assert_matches!(42f64.to_arg(), Arg::Float(_));
        assert_matches!('x'.to_arg(), Arg::UInt(120));
        let mut usize_val: usize = 0;
        assert_matches!((&mut usize_val).to_arg(), Arg::USizeRef(_));
        assert_matches!(42i8.to_arg(), Arg::SInt(42));
        assert_matches!(42i16.to_arg(), Arg::SInt(42));
        assert_matches!(42i32.to_arg(), Arg::SInt(42));
        assert_matches!(42i64.to_arg(), Arg::SInt(42));
        assert_matches!(42isize.to_arg(), Arg::SInt(42));

        assert_eq!((-42i8).to_arg(), Arg::SInt(-42));
        assert_eq!((-42i16).to_arg(), Arg::SInt(-42));
        assert_eq!((-42i32).to_arg(), Arg::SInt(-42));
        assert_eq!((-42i64).to_arg(), Arg::SInt(-42));
        assert_eq!((-42isize).to_arg(), Arg::SInt(-42));

        assert_matches!(42u8.to_arg(), Arg::UInt(42));
        assert_matches!(42u16.to_arg(), Arg::UInt(42));
        assert_matches!(42u32.to_arg(), Arg::UInt(42));
        assert_matches!(42u64.to_arg(), Arg::UInt(42));
        assert_matches!(42usize.to_arg(), Arg::UInt(42));

        let ptr = std::ptr::from_ref(&42f32);
        assert_matches!(ptr.to_arg(), Arg::UInt(_));
    }

    #[test]
    fn test_negative_to_arg() {
        assert_eq!((-1_i8).to_arg().as_sint(), Ok(-1));
        assert_eq!((-1_i16).to_arg().as_sint(), Ok(-1));
        assert_eq!((-1_i32).to_arg().as_sint(), Ok(-1));
        assert_eq!((-1_i64).to_arg().as_sint(), Ok(-1));

        assert_eq!((u64::MAX).to_arg().as_sint(), Err(Error::Overflow));
    }
}
