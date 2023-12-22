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

pub trait IsOkAnd {
    type Type;
    type Error;
    #[allow(clippy::wrong_self_convention)]
    fn is_ok_and(self, s: impl FnOnce(Self::Type) -> bool) -> bool;
    #[allow(clippy::wrong_self_convention)]
    fn is_err_and(self, s: impl FnOnce(Self::Error) -> bool) -> bool;
}
impl<T, E> IsOkAnd for Result<T, E> {
    type Type = T;
    type Error = E;
    fn is_ok_and(self, f: impl FnOnce(T) -> bool) -> bool {
        match self {
            Ok(v) => f(v),
            Err(_) => false,
        }
    }
    fn is_err_and(self, f: impl FnOnce(E) -> bool) -> bool {
        match self {
            Ok(_) => false,
            Err(e) => f(e),
        }
    }
}

pub trait IsSorted {
    type T;
    fn is_sorted_by(&self, pred: impl Fn(&Self::T, &Self::T) -> Option<std::cmp::Ordering>)
        -> bool;
}
impl<T> IsSorted for &[T] {
    type T = T;
    fn is_sorted_by(&self, pred: impl Fn(&T, &T) -> Option<std::cmp::Ordering>) -> bool {
        self.windows(2)
            .all(|w| pred(&w[0], &w[1]).is_none_or(|order| order.is_le()))
    }
}
impl<T> IsSorted for Vec<T> {
    type T = T;
    fn is_sorted_by(&self, pred: impl Fn(&T, &T) -> Option<std::cmp::Ordering>) -> bool {
        IsSorted::is_sorted_by(&self.as_slice(), pred)
    }
}
