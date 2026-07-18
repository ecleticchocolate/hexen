//@ expect val 42
enum Option[T] { T Some  None }
struct Pair[T, U] { T a  U b }
fn transform[T, U](Option[T] o, fn(T) U f, U def) U {
    match o {
        .Some(v) { return f(v) }
        .None { return def }
    }
    return def
}
fn pair_sum(Pair[u32, u32] p) u32 { return p.a + p.b }
fn main() i32 {
    Pair[u32, u32] p = {.a = 10, .b = 32}
    Option[Pair[u32, u32]] o = .Some(p)
    return (i32) transform(o, pair_sum, 0)
}
