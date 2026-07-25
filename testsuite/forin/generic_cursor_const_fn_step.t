//@ expect val 60
// Shared Cursor[E,S,F] with a per-container generic step function baked in as
// a const-generic value param (vec_step[T]) -- no named cursor struct. Two
// substitution gaps blocked this: Type_Substitute's TYPE_CONST_VALUE branch
// copied cval.pin verbatim (F's fn type kept abstract S/E while self.state
// was concrete), and ce_eval_ident's generic-fn-value fold didn't substitute
// explicit type args through the active generic frame before instantiating
// (vec_step[T] stayed abstract at Vec[i32] monomorphization time).
enum Option[T] { T Some  None }

fn vec_step[T](struct{T* data  u32 pos  u32 len}* s) Option[T*] {
    if s.pos >= s.len { return .None }
    T* p = &s.data[s.pos]
    s.pos = s.pos + 1
    return .Some(p)
}

struct Cursor[E, S, fn(S*) Option[E*] F] { S state }

impl Cursor[E, S, F] {
    fn next() Option[E] {
        Option[E*] r = F(&self.state)
        match r {
            .None { return .None }
            .Some(p) { return .Some(*p) }
        }
        return .None
    }
}

struct Vec[T] { T* data  u32 len }

impl Vec[T] {
    fn begin() Cursor[T, struct{T* data  u32 pos  u32 len}, vec_step[T]] {
        struct{T* data  u32 pos  u32 len} st = {.data = self.data, .pos = 0, .len = self.len}
        return {.state = st}
    }
}

fn main() i32 {
    i32[3] arr = {10, 20, 30}
    Vec[i32] v = {.data = arr, .len = 3}
    i32 sum = 0
    for i32 x in v { sum = sum + x }
    return sum
}
