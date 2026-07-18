//@ expect val 43
enum Option[T] { T Some  None }
struct Cell { fn(u32) u32 op  Option[u32] cached }
fn sq(u32 x) u32 { return x * x }
fn main() i32 {
    Cell[2][2] grid
    grid[1][1].op = sq
    grid[1][1].cached = .Some(7)
    u32 acc = grid[1][1].op(6)
    match grid[1][1].cached { .Some(v) { acc += v }  .None { } }
    return (i32) acc
}
