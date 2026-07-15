//@ expect val 40
enum Mode { u32 Fast  u32 Slow }
struct Runner { fn(u32) u32 run  Mode mode }
fn fast_fn(u32 x) u32 { return x * 4 }
fn main() i32 {
    Runner r = {.run = fast_fn, .mode = .Fast{1}}
    match r.mode {
        .Fast{_} { return (i32) r.run(10) }
        .Slow{_} { return -1 }
    }
    return -2
}
