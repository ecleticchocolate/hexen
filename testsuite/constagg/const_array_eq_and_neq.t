//@ expect val 10
fn check(u32[3] a) u32 {
    u32[3] target = {1, 2, 3}
    if a != target { return 0 }
    return 1
}
const u32 ok  = check({1, 2, 3})
const u32 bad = check({9, 9, 9})
fn main() u32 { return ok * 10 + bad }
