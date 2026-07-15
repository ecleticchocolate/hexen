//@ expect val 35
struct Pair[A,B]{A fst B snd}
fn main()i32{
    Pair[i32,i32] inner={.fst=10,.snd=20}
    Pair[Pair[i32,i32],i32] outer={.fst=inner,.snd=5}
    return outer.fst.fst+outer.fst.snd+outer.snd
}
