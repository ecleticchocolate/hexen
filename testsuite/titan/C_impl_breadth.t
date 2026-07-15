//@ expect val 266
// TITAN C — impl breadth: methods across every receiver form, generic arity,
// method-level extra generics, chains, structs-as-return, heap, loops calling
// methods, combined with the same arithmetic/control-flow axes as A/B.

struct Counter { u32 val }
impl Counter {
    fn inc() { self.val = self.val + 1 }
    fn add(u32 n) { self.val = self.val + n }
    fn get() u32 { return self.val }
}

struct Box[T] { T val }
impl Box[T] {
    fn get() T { return self.val }
    fn set(T v) { self.val = v }
    fn get2() T { return self.get() }       // method calling method (1 level)
    fn map[U](U v) U { return v }            // extra method-level generic beyond T
}

struct Pair[A,B] { A first  B second }
impl Pair[A,B] {
    fn get_first() A { return self.first }
    fn get_second() B { return self.second }
    fn combine[C](C extra) i32 { return (i32)self.first + (i32)self.second + extra }
}

struct Point { i32 x  i32 y }
impl Point {
    fn translate(i32 dx, i32 dy) Point {
        Point r = {.x = self.x + dx, .y = self.y + dy}
        return r
    }
    fn sum() i32 { return self.x + self.y }
}

struct Node { i32 val  i32 next_val }
impl Node {
    fn get() i32 { return self.val }
    fn set(i32 v) { self.val = v }
}

fn main() i32 {
    i64 total = 0

    // --- value receiver, mutation, getter ---
    Counter c = {.val = 0}
    c.inc()
    c.inc()
    c.add(5)
    total = total + (i64)c.get()             // 7

    // --- pointer receiver via explicit & ---
    Counter c2 = {.val = 100}
    Counter* pc = &c2
    pc.inc()
    pc.add(3)
    total = total + (i64)pc.get()            // +104 = 111

    // --- generic impl, single type param, method-calls-method chain ---
    Box[i32] bi = {.val = 21}
    total = total + (i64)bi.get2() * 2       // +42 = 153

    // --- generic impl, different concrete type, in a loop ---
    Box[u32] bu = {.val = 0}
    u32 i = 0
    while i < 5 {
        bu.set(bu.get() + i)
        i = i + 1
    }
    total = total + (i64)bu.get()            // 0+1+2+3+4=10 -> 163

    // --- method-level extra generic param (U) on top of struct's T ---
    total = total + (i64)bi.map((i32)9)           // +9 = 172

    // --- multigeneric struct impl, both A and B accessed ---
    Pair[i32,i32] p = {.first = 3, .second = 4}
    total = total + (i64)p.get_first() + (i64)p.get_second()   // +7 = 179

    // --- multigeneric struct + method's own extra type param ---
    total = total + (i64)p.combine((i32)10)       // 3+4+10=17 -> 196

    // --- method returning a struct, chained field access ---
    Point pt = {.x = 1, .y = 2}
    Point pt2 = pt.translate(10, 20)
    total = total + (i64)pt2.sum()           // +33 = 229

    // --- method on heap-allocated receiver, array of heap nodes via loop ---
    Node* nodes = new[3] Node
    for u32 k = 0 to 3 {
        nodes[k].set((i32)k * 10)
    }
    i32 nsum = 0
    for u32 k = 0 to 3 { nsum = nsum + nodes[k].get() }   // 0+10+20=30
    delete nodes
    total = total + (i64)nsum                // +30 = 259

    // --- struct field access still works alongside method calls (no collision) ---
    total = total + (i64)c.val               // direct field, c.val=7 -> +7 = 266

    return (i32) total
}
