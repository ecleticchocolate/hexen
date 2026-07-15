//@ expect val 46
fn quad(i32 x) i32[4] { return {x, x+1, x+2, x+3} }
fn main() i32 {
    fn(i32) i32[4] f = quad   // bare, no grouping needed — [4] binds inward
    i32[4] r = f(10)
    return r[0] + r[1] + r[2] + r[3]
}
