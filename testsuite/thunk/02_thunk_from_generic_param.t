//@ expect val 9
struct C { i32 v }
pub impl C { fn get() i32 { return 9 } }
fn thunk[T](void* p) i32 { T* o = (T*)p  return o.get() }
fn via[T](T* o) i32 {
    fn(void*) i32 f = thunk[T]
    return f((void*)o)
}
fn main() i32 { C c = { .v = 0 }  return via(&c) }
