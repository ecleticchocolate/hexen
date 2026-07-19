//@ expect val 288
// HKT works uniformly across every construct Type_Substitute already reaches,
// with no per-construct code -- union field, alias-of-anonymous-struct body,
// generic function param/return type, and an impl method on an HKT struct.
// This is the payoff of Type_Substitute being the single choke point every
// construct already funnels through: fixing it once (Type_Substitute's
// TYPE_ARRAY case, for the deferred M[T] application) reaches all of these
// for free.
struct Box[T] { T val }

union HKTU[M, T] { M[T] data  i64 raw }
fn u_val() i32 {
    HKTU[Box, i32] u
    u.data = { .val = 77 }
    return u.data.val
}

alias HKTA[M, T] = struct { M[T] data }
fn a_val() i32 {
    HKTA[Box, i32] a
    a.data = { .val = 33 }
    return a.data.val
}

fn wrap[M, T](T v) M[T] {
    M[T] r
    r.val = v
    return r
}
fn fn_val() i32 {
    Box[i32] b = wrap[Box, i32](55)
    return b.val
}

struct HKT[M, T] { M[T] data }
impl HKT[M, T] {
    fn get() T { return self.data.val }
    fn set(T v) { self.data.val = v }
}
fn impl_val() i32 {
    HKT[Box, i32] h
    h.set(123)
    return h.get()
}

fn main() i32 {
    return u_val() + a_val() + fn_val() + impl_val()   // 77+33+55+123 = 288
}
