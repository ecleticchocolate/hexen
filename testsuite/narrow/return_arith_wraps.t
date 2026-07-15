//@ expect val 4
fn f() u8 {
    u8 x = 250
    return x + 10
}
fn main() i32 { return (i32)f() }
