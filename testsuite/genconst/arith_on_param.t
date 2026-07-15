//@ expect val 10
struct Fixed[T, u32 N] { T[N] data } impl Fixed[T, u32 N] { fn doubled() u32 { const u32 c = N * 2 return c } } fn main() i32 { Fixed[i32, 5] f return (i32)f.doubled() }
