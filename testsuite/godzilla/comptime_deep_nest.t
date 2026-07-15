//@ expect val 48
struct Wrap[T] { T inner  u32 depth }
fn deep() u32 {
    Wrap[Wrap[Wrap[u32]]] w = {.inner = {.inner = {.inner = 42, .depth = 3}, .depth = 2}, .depth = 1}
    return w.inner.inner.inner + w.depth + w.inner.depth + w.inner.inner.depth
}
const u32 D = deep()
fn main() i32 { return (i32) D }
