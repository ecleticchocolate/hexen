//@ expect val 269
struct Counter[T, u32 Step] { T val }
impl Counter[T, u32 Step] {
    fn inc() { self.val = self.val + Step }
    fn add(T n) { self.val = self.val + n * Step }
    fn get() T { return self.val }
    fn bump2() { self.inc(); self.inc() }
}

struct Point[T, u32 Dim] { T[Dim] coords }
impl Point[T, u32 Dim] {
    fn translate(T[Dim] delta) Point[T, Dim] {
        Point[T, Dim] r
        u32 i = 0
        while i < Dim {
            r.coords[i] = self.coords[i] + delta[i]
            i = i + 1
        }
        return r
    }
    fn sum() T {
        T total = 0
        u32 i = 0
        while i < Dim {
            total = total + self.coords[i]
            i = i + 1
        }
        return total
    }
}

struct Box[T, u32 N] { T[N * 2] v }
impl Box[T, u32 N] {
    fn set(T x) { 
        u32 i = 0
        while i < N * 2 {
            self.v[i] = x
            i = i + 1
        }
    }
    fn get() T { return self.v[N] }
}

fn compute() i32 {
    i32 total = 0

    Counter[u32, 5] c = {.val = 0}
    c.inc()
    c.inc()
    c.add(2)
    total = total + (i32)c.get()            // 20

    Counter[u32, 10] c2 = {.val = 100}
    Counter[u32, 10]* pc = &c2
    pc.inc()
    pc.add(3)
    total = total + (i32)pc.get()           // 20 + 140 = 160

    Counter[u32, 1] c3 = {.val = 0}
    c3.bump2()
    c3.bump2()
    total = total + (i32)c3.get()           // 160 + 4 = 164

    Point[i32, 3] p
    p.coords[0] = 1; p.coords[1] = 2; p.coords[2] = 3
    i32[3] delta = {10, 20, 30}
    Point[i32, 3] p2 = p.translate(delta)
    total = total + p2.sum()                // 164 + 66 = 230

    Box[u32, 3] b
    b.set(5)
    u32 i = 0
    while i < 5 { b.set(b.get() + i); i = i + 1 }
    total = total + (i32)b.get()            // 230 + 15 = 245
    total = total + (i32)sizeof(b)          // Box[u32, 3] has u32[6] -> 24 bytes -> 245 + 24 = 269

    return total
}
fn main() i32 { return compute() }
