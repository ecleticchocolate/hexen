//@ expect err type mismatch in assignment: cannot assign u32[2][3] to u32[2][4]
fn main() i32 {
    u32[2][3] grid = {{1,2,3},{4,5,6}}
    u32[2][4] wrong
    wrong = grid
    return 0
}
