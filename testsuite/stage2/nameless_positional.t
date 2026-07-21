//@ expect val 41
fn p[T]() i32 {
    match T { struct { A; B } { return (i32)sizeof(A)*10 + (i32)sizeof(B) } else { return 2 } }
    return -1
}
fn main() i32 { return p[struct{i32 x  u8 y}]() }
