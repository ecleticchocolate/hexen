//@ expect val 814
struct Wrap[T, u32 N] { T[N] buf }
struct Pair[A, B] { A first  B second }
fn main() i32 {
    match Pair[u16*, Wrap[u8, 4]] {
        Pair[E, Wrap[F, N]] { return (i32)sizeof(E)*100 + (i32)sizeof(F)*10 + (i32)N }
        else { return 0 }
    }
}
