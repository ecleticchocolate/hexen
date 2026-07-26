//@ expect val 0
fn main() i32 {
    static_assert(1 == 1)
    static_assert(sizeof(i32) == 4, "i32 must be 4 bytes")
    return 0
}
