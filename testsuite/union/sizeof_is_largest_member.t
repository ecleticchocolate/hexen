//@ expect val 8
union U { u8 a  i64 b  u16 c }
fn main() i32 {
    return (i32) sizeof(U)
}
