//@ expect val 211
// `Ts...` CONSTRUCTS a carrier from the arguments supplied at the application
// site. It never passes an argument through, so the arity of an application is
// simply how many arguments were written -- never a function of whether one of
// them happened to be anonymous.
//
// `Def[i32, u8]`         -> 2 args -> carrier struct{i32,u8}          -> arity 2
// `Def[struct{i32;u8}]`  -> 1 arg  -> carrier struct{struct{i32,u8}}  -> arity 1
// `Def[Point]`           -> 1 arg  -> carrier struct{Point}           -> arity 1
//
// An earlier cut collapsed a lone ANONYMOUS argument into the carrier so that
// the two spellings named one type. That made a one-argument application report
// arity 2 while `Def[Point]` -- structurally the same application -- reported 1:
// the same syntax meaning different things based on the argument's anonymity.
// The two spellings are therefore DIFFERENT types, and that is the point.
struct Point { i32 x  i32 y }
struct Def[Ts...] { Ts field  u32 n }
fn ident[A, B]() i32 { match A { B { return 1 } else { return 0 } } }
fn nf[T]() u32 { match T { struct { H; Rest... } { return 1 + nf[Rest]() } else { return 0 } } }
fn arity[X]() u32 { match X { Def[E] { return nf[E]() } else { return 999 } } }
fn main() i32 {
    u32 r = arity[Def[i32,u8]]() * 100          // 200
    r = r + arity[Def[struct{i32;u8}]]() * 10   // 10
    r = r + arity[Def[Point]]()                 // 1
    // and the two spellings must NOT be the same type
    r = r + (u32)ident[Def[i32,u8], Def[struct{i32;u8}]]()   // + 0
    return (i32)r
}
