//@ expect val 10
struct Dyn[V] { void* obj  V vt }
fn dyn[T, V](T* o, V... fns) Dyn[V] { return { .obj = (void*)o, .vt = fns } }
alias Getter = struct { (fn(void*) i32) get }
fn t_get[T](void* p) i32 { T* o = (T*)p  return o.get() }
struct A { i32 v }
pub impl A { fn get() i32 { return 3 } }
struct B { u8* s }
pub impl B { fn get() i32 { return 7 } }
fn main() i32 {
    A a = { .v = 0 }
    B b = { .s = "x" }
    Dyn[Getter][2] items
    items[0] = dyn[A, Getter](&a, t_get[A])
    items[1] = dyn[B, Getter](&b, t_get[B])
    i32 sum = 0
    for u32 i = 0 to 2 { sum = sum + items[i].vt.get(items[i].obj) }
    return sum
}
