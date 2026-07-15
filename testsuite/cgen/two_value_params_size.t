//@ expect val 123
struct Mat[T, u32 R, u32 C] { T[R * C] cells }
impl Mat[T, u32 R, u32 C] {
    fn at(u32 r, u32 c) T { return self.cells[r * C + c] }
    fn set(u32 r, u32 c, T v) { self.cells[r * C + c] = v }
}
fn main() i32 {
    Mat[i32, 2, 3] m
    m.set(1, 2, 99)
    return m.at(1, 2) + (i32)sizeof(m)   // 99 + 24 = 123
}
