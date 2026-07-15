//@ expect val 53
// `super` x dyn composition. super copies FIELDS only -- Derived does NOT
// inherit Base's methods, and does not satisfy `impl { fn get() i32 }` by
// virtue of Base doing so. But `super Base base` written FIRST gives Derived
// a layout PREFIX identical to Base (the promoted fields sit at the same
// offsets), which is a real guarantee, not sugar: a Derived* upcast to Base*
// is readable through Base's own method. So one Derived object can be packed
// into a Dyn[Getter] TWO ways -- through its own thunk (its own method) or
// through Base's thunk after an upcast (the base's method reading the shared
// prefix) -- and both must produce correct, different answers.
struct Base { i32 x }
pub impl Base { fn get() i32 { return self.x } }

struct Derived {
    super Base base
    i32 extra
}
pub impl Derived { fn get() i32 { return self.x + self.extra } }

struct Dyn[V] { void* obj  V vt }
fn dyn[T, V](T* o, V... fns) Dyn[V] { return { .obj = (void*)o, .vt = fns } }
alias Getter = struct { (fn(void*) i32) get }
fn t_get[T](void* p) i32 { T* o = (T*)p  return o.get() }

fn main() i32 {
    Base b = { .x = 10 }
    Derived d
    d.x = 20        // promoted field -- offset 0, same as Base's x
    d.extra = 3

    Dyn[Getter][3] items
    items[0] = dyn[Base, Getter](&b, t_get[Base])              // 10
    items[1] = dyn[Derived, Getter](&d, t_get[Derived])        // 20 + 3 = 23
    items[2] = dyn[Base, Getter]((Base*)&d, t_get[Base])       // upcast: Base_get reads d's prefix -> 20

    i32 sum = 0
    for u32 i = 0 to 3 { sum = sum + items[i].vt.get(items[i].obj) }
    return sum      // 10 + 23 + 20 = 53
}
