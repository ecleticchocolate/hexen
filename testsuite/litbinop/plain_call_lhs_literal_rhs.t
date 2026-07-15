//@ expect val 1
fn make() u32[4] { return {1, 2, 3, 4} }
fn main() u32 {
    if make() == {1, 2, 3, 4} { return 1 }
    return 0
}
