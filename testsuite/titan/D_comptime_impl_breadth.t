//@ expect val 200
struct Counter { u32 val }
impl Counter {
    fn inc() { self.val = self.val + 1 }
    fn add(u32 n) { self.val = self.val + n }
    fn get() u32 { return self.val }
    fn bump2() { self.inc(); self.inc() }        // method calling method (mutating)
}

struct Point { i32 x  i32 y }
impl Point {
    fn translate(i32 dx, i32 dy) Point {         // method returning a struct
        Point r = {.x = self.x + dx, .y = self.y + dy}
        return r
    }
    fn sum() i32 { return self.x + self.y }
}

struct Box { u32 v }
impl Box {
    fn set(u32 x) { self.v = x }
    fn get() u32 { return self.v }
}

fn compute() i32 {
    i32 total = 0

    // value receiver: void mutation + arg-taking mutation + getter
    Counter c = {.val = 0}
    c.inc()
    c.inc()
    c.add(5)
    total = total + (i32)c.get()            // 7

    // pointer receiver via explicit & — writes go through the pointer
    Counter c2 = {.val = 100}
    Counter* pc = &c2
    pc.inc()
    pc.add(3)
    total = total + (i32)pc.get()           // +104 = 111

    // method-calls-method, both mutating through self
    Counter c3 = {.val = 0}
    c3.bump2()
    c3.bump2()
    total = total + (i32)c3.get()           // +4 = 115

    // method returning a struct, chained field access
    Point p = {.x = 1, .y = 2}
    Point p2 = p.translate(10, 20)
    total = total + p2.sum()                // +33 = 148

    // loop repeatedly calling set/get on the same receiver
    Box b = {.v = 0}
    u32 i = 0
    while i < 5 { b.set(b.get() + i); i = i + 1 }
    total = total + (i32)b.get()            // 0+1+2+3+4=10 -> 158

    // &scalar-local + pointer-to-scalar write (unified store: scalars have addresses)
    u32 x = 41
    u32* px = &x
    px[0] = px[0] + 1
    total = total + (i32)x                  // +42 = 200

    return total
}

const i32 TITAND = compute()
fn main() i32 { return TITAND }   // 200
