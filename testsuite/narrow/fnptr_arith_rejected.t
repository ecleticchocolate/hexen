//@ expect err arithmetic on a function pointer
fn f() i32 { return 1 }
fn main() i32 {
    fn() i32 p = f
    fn() i32 r = p + 1
    r()
    return 0
}
