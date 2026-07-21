//@ expect val 51
// Because `Ts` is an ordinary TYPE param bound to the bundle, `impl Def[T]`
// covers every arity at once -- no per-arity impl blocks.
struct Def[Ts...] { Ts field  u32 n }
impl Def[T] { fn nn() u32 { return self.n } }
fn main() i32 {
    Def[i32, u8] d          d.n = 42
    Def[i32, u8, f64, u16] e  e.n = 9
    return (i32)(d.nn() + e.nn())
}
