//@ expect val 5
struct C { i32 v }
pub impl C { fn get() i32 { return 5 } }
struct N { i32 v }
fn thunk[T](void* p) i32 { T* o = (T*)p  return o.get() }
fn via[T](T* o) i32 {
    match T {
        impl { fn get() i32 } { fn(void*) i32 f = thunk[T]  return f((void*)o) }
        else { return 0 }
    }
}
fn main() i32 { C c = { .v = 0 }  return via(&c) }
