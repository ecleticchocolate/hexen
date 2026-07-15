//@ expect val 6
struct Vec[T, u32 N] { T[N] e }
impl Vec[T, u32 N] {
    fn copy_prefix[u32 M](Vec[T, M]* dst) {
        u32 i = 0
        while i < M && i < N { dst.e[i] = self.e[i]  i = i + 1 }
    }
}
fn main() i32 {
    Vec[i32, 5] a
    a.e[0]=1 a.e[1]=2 a.e[2]=3 a.e[3]=4 a.e[4]=5
    Vec[i32, 3] b
    a.copy_prefix(&b)
    return b.e[0] + b.e[1] + b.e[2]
}
