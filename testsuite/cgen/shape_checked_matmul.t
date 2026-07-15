//@ expect val 415
struct Mat[T, u32 R, u32 C] { T[R * C] e }
impl Mat[T, u32 R, u32 C] {
    fn at(u32 r, u32 c) T { return self.e[r * C + c] }
    fn set(u32 r, u32 c, T v) { self.e[r * C + c] = v }
}
fn matmul[T, u32 R, u32 K, u32 Cc](Mat[T, R, K] a, Mat[T, K, Cc] b) Mat[T, R, Cc] {
    Mat[T, R, Cc] out
    u32 i = 0
    while i < R {
        u32 j = 0
        while j < Cc {
            T acc = 0  u32 k = 0
            while k < K { acc = acc + a.at(i, k) * b.at(k, j)  k = k + 1 }
            out.set(i, j, acc)  j = j + 1
        }
        i = i + 1
    }
    return out
}
fn main() i32 {
    Mat[i32, 2, 3] a
    a.set(0,0,1) a.set(0,1,2) a.set(0,2,3)
    a.set(1,0,4) a.set(1,1,5) a.set(1,2,6)
    Mat[i32, 3, 2] b
    b.set(0,0,7)  b.set(0,1,8)
    b.set(1,0,9)  b.set(1,1,10)
    b.set(2,0,11) b.set(2,1,12)
    Mat[i32, 2, 2] c = matmul(a, b)
    return c.at(0,0) + c.at(0,1) + c.at(1,0) + c.at(1,1)   // 415
}
