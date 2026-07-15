//@ expect val 7
struct Fixed[T, u32 N] { T[N] data } impl Fixed[T, u32 N] { fn seven() u32 { const u32 k = 7 return k } } fn main() i32 { Fixed[i32, 5] f return (i32)f.seven() }
