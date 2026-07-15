//@ expect val 180
struct Ring[T, u32 CAP] { T[CAP] data  u32 head  u32 count }
impl Ring[T, u32 CAP] {
    fn push(T v) {
        u32 idx = (self.head + self.count) % CAP
        self.data[idx] = v
        if self.count < CAP { self.count = self.count + 1 }
        else { self.head = (self.head + 1) % CAP }
    }
    fn sum() T {
        T total = 0
        u32 i = 0
        while i < self.count { total = total + self.data[(self.head + i) % CAP]  i = i + 1 }
        return total
    }
}
fn build() Ring[i32, 4] {
    Ring[i32, 4] r
    r.head = 0
    r.count = 0
    r.push(10)  r.push(20)  r.push(30)  r.push(40)  r.push(50)  r.push(60)
    return r
}
const Ring[i32, 4] R = build()
fn main() i32 { return R.sum() }
