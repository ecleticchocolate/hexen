//@ expect val 13
struct Fixed[T, u32 N] { T[N] data } impl Fixed[T, u32 N] { fn direct() u32 { const u32 c = N return c } } fn main() i32 { Fixed[i32, 5] a Fixed[i32, 8] b return (i32)(a.direct() + b.direct()) }
