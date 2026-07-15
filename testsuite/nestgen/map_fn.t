//@ expect val 13
struct Pair[A,B]{A fst B snd}
fn map_fst[A,B](Pair[A,B] p,fn(A)A f)Pair[A,B]{return {.fst=f(p.fst),.snd=p.snd}}
fn dbl(i32 x)i32{return x*2}
fn main()i32{
    Pair[i32,i32] p={.fst=5,.snd=3}
    Pair[i32,i32] q=map_fst(p,dbl)
    return q.fst+q.snd
}
