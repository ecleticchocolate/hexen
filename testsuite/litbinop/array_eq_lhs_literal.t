//@ expect val 1
fn main() u32 {
    u32[4] a = {1, 2, 3, 4}
    if {1, 2, 3, 4} == a { return 1 }
    return 0
}
