//@ expect val 84
struct S { u64* p  i32 n }
fn main() i32 {
    match S {
        struct { E*; B } { return (i32)sizeof(E)*10 + (i32)sizeof(B) }
        else { return 99 }
    }
}
