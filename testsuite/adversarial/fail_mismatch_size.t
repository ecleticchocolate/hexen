//@ expect err type mismatch in assignment: cannot assign u32[4] to u32[8]
fn main() i32 {
    u32[4] e = {1,2,3,4}
    u32[8] f
    f = e
    return 0
}
