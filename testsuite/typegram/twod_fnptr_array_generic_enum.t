//@ expect val 9
enum Option[T] { T Some  None }
fn ok(u32 x) Option[u32] { return .Some(x) }
fn main() i32 {
    (fn(u32) Option[u32])[2][2] grid
    grid[1][1] = ok
    Option[u32] r = grid[1][1](9)
    match r { .Some(v) { return (i32) v }  .None { return 0 } }
    return 0
}
