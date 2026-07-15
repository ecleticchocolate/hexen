//@ expect val 0
fn main() u32 {
    u32[4] a = {1, 2, 3, 4}
    if a == {9, 9, 9, 9} { return 1 }
    return 0
}
