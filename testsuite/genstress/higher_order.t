//@ expect val 42
fn apply[T](fn(T) T f, T x) T { return f(x) }
fn apply2[T](fn(T,T) T f, T a, T b) T { return f(a, b) }
fn compose[T](fn(T) T f, fn(T) T g, T x) T { return f(g(x)) }
fn inc(i32 x) i32 { return x + 1 }
fn dbl(i32 x) i32 { return x * 2 }
fn add(i32 a, i32 b) i32 { return a + b }
fn main() i32 {
    i32 a = apply(inc, 41)
    i32 b = apply2(add, 20, 22)
    i32 c = compose(dbl, inc, 20)
    return a + b + c - 84
}
