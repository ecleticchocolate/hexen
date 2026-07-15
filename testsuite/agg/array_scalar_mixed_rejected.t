//@ expect err mismatched operand types
fn main() u32 {
    u32[4] a = {1, 2, 3, 4}
    u32 s = 10
    u32[4] r = a * s
    return r[0]
}
