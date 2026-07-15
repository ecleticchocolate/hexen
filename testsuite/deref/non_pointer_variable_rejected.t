//@ expect err cannot dereference non-pointer type
fn main() i32 {
    i32 y = 7
    i32 x = *y
    return x
}
