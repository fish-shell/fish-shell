//! stdlib backports

pub trait IsSomeAnd {
    type Type;
    #[allow(clippy::wrong_self_convention)]
    fn is_some_and(self, s: impl FnOnce(Self::Type) -> bool) -> bool;
    #[allow(clippy::wrong_self_convention)]
    fn is_none_or(self, s: impl FnOnce(Self::Type) -> bool) -> bool;
}

impl<T> IsSomeAnd for Option<T> {
    type Type = T;
    fn is_some_and(self, f: impl FnOnce(T) -> bool) -> bool {
        match self {
            Some(v) => f(v),
            None => false,
        }
    }
    fn is_none_or(self, f: impl FnOnce(T) -> bool) -> bool {
        match self {
            Some(v) => f(v),
            None => true,
        }
    }
}
