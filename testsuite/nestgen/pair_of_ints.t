//@ expect val 42
struct Pair[A,B]{A fst B snd}
fn main()i32{
    Pair[i32,i32] p={.fst=10,.snd=32}
    return p.fst+p.snd
}
