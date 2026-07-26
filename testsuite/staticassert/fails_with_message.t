//@ expect err static assertion failed: one is not two
fn main() i32 {
    static_assert(1 == 2, "one is not two")
    return 0
}
