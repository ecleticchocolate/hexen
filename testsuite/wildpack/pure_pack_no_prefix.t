//@ expect stdout
//@ | struct{i32,u8}
// No fixed prefix at all -- the whole bundle rebinds to Rest.
struct Def2[Ts...] { Ts field }
extern fn printf(u8* fmt, ...) i32
fn f[S]() i32 {
    match S {
        struct M[Rest...] { printf("%s\n", nameof(Rest))  return 1 }
        else { return -100 }
    }
    return -2
}
fn main() i32 { return f[Def2[i32, u8]]() }
