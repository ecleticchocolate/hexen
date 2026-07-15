//@ expect val 42
struct Box[T] { T val }
fn dbl(u32 x) u32 { return x * 2 }
fn main() i32 {
    (fn(u32) u32)[2] a
    a[0] = dbl
    a[1] = dbl
    Box[(fn(u32) u32)[2]] b = {.val = a}
    return (i32)(b.val[0](21))
}
