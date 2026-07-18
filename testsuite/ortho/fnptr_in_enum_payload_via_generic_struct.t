//@ expect val 21
struct Handler[T] { fn(T) T transform  T val }
enum Cmd { Handler[u32] Run  u32 Skip }
fn triple(u32 x) u32 { return x * 3 }
fn main() i32 {
    Handler[u32] h = {.transform = triple, .val = 7}
    Cmd c = .Run(h)
    match c {
        .Run(handler) { return (i32) handler.transform(handler.val) }
        .Skip(v) { return (i32) v }
    }
    return -1
}
