//@ expect err type redefinition
union U { i32 x }
union U { i32 y }
fn main() i32 {
    return 0
}
