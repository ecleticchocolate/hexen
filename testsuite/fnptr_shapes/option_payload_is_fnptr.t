//@ expect val 42
enum Opt[T] { T Some  None }
fn add1(u32 x) u32 { return x + 1 }
fn main() i32 {
    Opt[fn(u32) u32] o = .Some{add1}
    match o {
        .Some{f} { return (i32)f(41) }
        .None { return -1 }
    }
}
