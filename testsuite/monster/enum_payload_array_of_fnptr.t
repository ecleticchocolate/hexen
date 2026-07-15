//@ expect val 20
struct Fns { (fn(u32) u32)[2] arr }
enum Cmd { Fns Batch  u32 Single }
fn inc(u32 x) u32 { return x + 1 }
fn dec(u32 x) u32 { return x - 1 }
fn main() i32 {
    Fns f
    f.arr[0] = inc
    f.arr[1] = dec
    Cmd c = .Batch{f}
    match c {
        .Batch{fs} { return (i32)(fs.arr[0](10) + fs.arr[1](10)) }
        .Single{v} { return (i32) v }
    }
    return -1
}
