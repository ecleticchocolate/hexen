//@ expect val 42
struct A { i32 v }
struct B { A inner }
struct C { B mid }
fn main() i32 {
    C c = {.mid = {.inner = {.v = 42}}}
    return c.mid.inner.v
}
