//@ expect val 42
struct Box[T] { T val }
fn double(u32 x) u32 { return x * 2 }
fn main() i32 {
    Box[fn(u32) u32] b = {.val = double}
    return (i32) b.val(21)
}
