//@ expect err cannot dereference non-pointer type
fn main() i32 {
    i32 x = 7
    *x = 99
    return x
}
