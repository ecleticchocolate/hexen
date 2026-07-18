//@ expect val 41
enum Opt[T] { T Some  None }
fn f0(u32 x) Opt[u32] { return .Some(x + 1) }
fn f1(u32 x) Opt[u32] { if x > 100 { return .None }  return .Some(x * 2) }
fn f2(u32 x) Opt[u32] { return .Some(x * x) }
struct Dispatch { (fn(u32) Opt[u32])[3] handlers }
fn main() i32 {
    Dispatch d
    d.handlers[0] = f0
    d.handlers[1] = f1
    d.handlers[2] = f2
    u32 acc = 0
    for u32 i = 0 to 3 {
        match d.handlers[i](5) { .Some(v) { acc = acc + v }  .None { } }
    }
    return (i32) acc
}
