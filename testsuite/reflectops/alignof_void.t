//@ expect val 0
struct WithVoid { i32 a  void v  i32 b }
fn main() i32 {
    if (alignof(void) != 1) { return 1 }
    if (sizeof(WithVoid) != 8) { return 2 }
    return 0
}
