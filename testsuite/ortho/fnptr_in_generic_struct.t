//@ expect val 42
struct Handler[T] { fn(T) T apply  T val }
fn double_u32(u32 x) u32 { return x * 2 }
fn main() i32 {
    Handler[u32] h = {.apply = double_u32, .val = 21}
    return (i32) h.apply(h.val)
}
