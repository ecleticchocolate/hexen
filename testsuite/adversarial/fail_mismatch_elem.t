//@ expect err type mismatch in assignment: cannot assign u32[4] to i32[4]
fn main() i32 {
    u32[4] c = {1,2,3,4}
    i32[4] d
    d = c
    return 0
}
