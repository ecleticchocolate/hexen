//@ expect val 7
struct C { i32 v }
pub impl C { fn get() i32 { return 7 } }
fn thunk[T](void* p) i32 { T* o = (T*)p  return o.get() }
fn main() i32 {
    C c = { .v = 0 }
    fn(void*) i32 f = thunk[C]
    return f((void*)&c)
}
