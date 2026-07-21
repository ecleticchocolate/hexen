//@ expect val 8
struct Tri[A,B,C] { A a  B b  C c }
fn p[S]() i32 {
    match S { struct M[X,Y,Z] { Z v = 0  return (i32)sizeof(v) } else { return 2 } }
    return -1
}
fn main() i32 { return p[Tri[i32,u8,u64]]() }
