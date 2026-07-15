//@ expect val 1
fn main() u32 {
    u32[4] a = {1, 2, 3, 4}
    if a == {1, 2, 3, 4} { return 1 }
    return 0
}
