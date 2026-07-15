//@ expect val 24
struct Small { u8 a }
struct Big { i64 a  i64 b  i64 c }
union U { Small s  Big b }
fn main() i32 {
    return (i32) sizeof(U)
}
