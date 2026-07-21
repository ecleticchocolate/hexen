//@ expect val 12
union U { i32 i  u8* s }
union V { i32 zzz  u8* s }
fn probe[T]() i32 {
    match T { union { i32 i  u8* s } { return 1 } else { return 2 } }
    return -1
}
fn main() i32 { return probe[U]()*10 + probe[V]() }
