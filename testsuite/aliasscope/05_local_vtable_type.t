//@ expect val 10
struct A { i32 v }
pub impl A { fn get() i32 { return 3 } }
struct B { i32 v }
pub impl B { fn get() i32 { return 7 } }
struct Dyn[V] { void* obj  V vt }
fn t_get[T](void* p) i32 { T* o = (T*)p  return o.get() }
fn run() i32 {
    alias GetVT = struct { (fn(void*) i32) get }
    A a = { .v = 0 }
    B b = { .v = 0 }
    Dyn[GetVT][2] items
    items[0] = { .obj = (void*)&a, .vt = { .get = t_get[A] } }
    items[1] = { .obj = (void*)&b, .vt = { .get = t_get[B] } }
    i32 sum = 0
    for u32 i = 0 to 2 { sum = sum + items[i].vt.get(items[i].obj) }
    return sum
}
fn main() i32 { return run() }
