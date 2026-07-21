//@ expect val 44
struct S { f32[4] arr }
fn main() i32 {
    match S {
        struct { E[N] } { return (i32)N*10 + (i32)sizeof(E) }
        else { return 99 }
    }
}
