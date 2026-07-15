//@ expect val 55
struct Box[T] { T val }
struct Wrapper[T] { Box[T] inner }
fn main() i32 {
    Wrapper[u32] w = {.inner = {.val = 55}}
    return (i32) w.inner.val
}
