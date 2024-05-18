//! stdlib backports

pub trait IsSomeAnd {
    type Type;
    #[allow(clippy::wrong_self_convention)]
    fn is_none_or(self, s: impl FnOnce(Self::Type) -> bool) -> bool;
}
impl<T> IsSomeAnd for Option<T> {
    type Type = T;
    fn is_none_or(self, f: impl FnOnce(T) -> bool) -> bool {
        match self {
            Some(v) => f(v),
            None => true,
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
