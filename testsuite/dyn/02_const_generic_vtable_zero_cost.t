//@ expect val 8
struct Circle { i32 r }
pub impl Circle { fn area() i32 { return 42 } }
fn t_area[T](void* p) i32 { T* o = (T*)p  return o.area() }
struct Dyn[fn(void*) i32 A] { void* obj }
pub impl Dyn[A] { fn area() i32 { return A(self.obj) } }
fn main() i32 {
    Circle c = { .r = 1 }
    Dyn[t_area[Circle]] d = { .obj = (void*)&c }
    return (i32)sizeof(d)
}
