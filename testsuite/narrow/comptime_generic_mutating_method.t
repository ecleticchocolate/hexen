//@ expect val 100
struct Box[T, u32 N] { T[N] data  u32 count }
impl Box[T, u32 N] {
    fn push(T v) {
        self.data[self.count] = v
        self.count = self.count + 1
    }
}
fn build() Box[i32, 4] {
    Box[i32, 4] b
    b.count = 0
    b.push(99)
    return b
}
const Box[i32, 4] B = build()
fn main() i32 { return B.data[0] + (i32)B.count }
