//@ expect val 10
struct Pair[T, U] { T a  U b }
enum Wrap { Pair[u32, bool] Tagged  u32 Plain }
fn main() i32 {
    Wrap w = .Tagged( {.a = 10, .b = true} )
    match w {
        .Tagged(pr) { return (i32) pr.a }
        .Plain(v) { return (i32) v }
    }
    return -1
}
