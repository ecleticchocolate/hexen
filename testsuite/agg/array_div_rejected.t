//@ expect err aggregate operands
fn main() u32 {
    u32[2] a = {10, 20}
    u32[2] b = {2, 4}
    u32[2] c = a / b
    return c[0]
}
