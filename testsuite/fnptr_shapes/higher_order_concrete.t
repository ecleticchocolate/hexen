//@ expect val 10
fn add1(i32 x) i32 { return x + 1 }
fn apply(fn(i32) i32 f, i32 x) i32 { return f(x) }
fn main() i32 {
    fn(fn(i32) i32, i32) i32 g = apply
    return g(add1, 9)
}
