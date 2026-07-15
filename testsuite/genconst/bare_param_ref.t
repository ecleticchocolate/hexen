//@ expect val 5
struct Fixed[T, u32 N] { T[N] data } impl Fixed[T, u32 N] { fn direct() u32 { const u32 c = N return c } } fn main() i32 { Fixed[i32, 5] f return (i32)f.direct() }
