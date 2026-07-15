//@ expect val 11
fn inc(i32* p) void { *p = *p + 1 }
fn main() i32 {
    fn(i32*) void f = inc
    i32 x = 10
    f(&x)
    return x
}
