//@ expect val 0
fn f() i32 { return 1 }
fn g() i32 { return 2 }
fn main() i32 {
    fn() i32 p = f
    fn() i32 q = g
    bool same = p == q
    return (i32)same
}
