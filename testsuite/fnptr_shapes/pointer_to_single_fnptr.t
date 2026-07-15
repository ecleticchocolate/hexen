//@ expect val 20
fn add1(i32 x) i32 { return x + 1 }
fn double(i32 x) i32 { return x * 2 }
fn main() i32 {
    fn(i32) i32 f = add1
    (fn(i32) i32)* p = &f
    *p = double
    return f(10)
}
