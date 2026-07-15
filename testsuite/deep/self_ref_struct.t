//@ expect val 3
struct N { u32 v  N* next }
fn main() i32 {
    N a = {.v = 1, .next = null}
    N b = {.v = 2, .next = &a}
    return (i32)(b.v + b.next.v)
}
