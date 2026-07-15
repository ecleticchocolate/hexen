//@ expect val 42
struct B[T] { T v }
fn main() i32 {
    B[B[B[B[B[u32]]]]] x = {.v = {.v = {.v = {.v = {.v = 42}}}}}
    return (i32) x.v.v.v.v.v
}
