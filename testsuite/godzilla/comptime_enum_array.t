//@ expect val 120
enum Opt[T] { T Some  None }
fn dbl(u32 x) Opt[u32] { return .Some{x * 2} }
fn compute() u32 {
    Opt[u32][3] results = { dbl(10), dbl(20), dbl(30) }
    u32 acc = 0
    for u32 i = 0 to 3 {
        match results[i] { .Some{v} { acc = acc + v }  .None { } }
    }
    return acc
}
const u32 R = compute()
fn main() i32 { return (i32) R }
