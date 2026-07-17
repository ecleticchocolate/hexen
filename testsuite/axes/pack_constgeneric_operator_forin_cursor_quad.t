//@ expect val 33
// Quad axis: variadic pack (T...) peeled at the type level to COUNT args,
// that count is used as a const-generic array size (u32 N), the struct
// carries an OPERATOR OVERLOAD (__add combines two Ops[N] by summing their
// per-slot bias values, real semantic work, not a token gesture), and the
// combined result is iterated via the for-in CURSOR protocol, calling each
// stored fnptr with its slot's bias added.
enum Option[T] { T Some  None }

fn pack_count[T](u32 acc) u32 {
    match T {
        struct {} { return acc }
        struct { H head  Rest... rest } { return pack_count[Rest](acc + 1) }
    }
}
fn count_args[T](T... args) u32 { return pack_count[T](0) }

struct Ops[u32 N] { (fn(i32) i32)[N] fns  i32[N] bias  u32 pos }
struct Cur[u32 N] { (fn(i32) i32)[N] fns  i32[N] bias  u32 pos }
impl Ops[u32 N] {
    fn begin() Cur[N] { return {.fns = self.fns, .bias = self.bias, .pos = 0} }
    fn __add(Ops[N] other) Ops[N] {
        Ops[N] r = {.fns = self.fns, .bias = self.bias, .pos = 0}
        u32 i = 0
        while i < N { r.bias[i] = self.bias[i] + other.bias[i]  i = i + 1 }
        return r
    }
}
impl Cur[u32 N] {
    fn next() Option[i32] {
        if self.pos >= N { return .None }
        i32 v = self.fns[self.pos]((i32)N) + self.bias[self.pos]
        self.pos = self.pos + 1
        return .Some{v}
    }
}

fn double(i32 x) i32 { return x * 2 }
fn triple(i32 x) i32 { return x * 3 }
fn quad(i32 x) i32 { return x * 4 }

fn main() i32 {
    u32 n = count_args(double, triple, quad)   // n == 3
    Ops[3] a = {.fns = {double, triple, quad}, .bias = {1, 1, 1}, .pos = 0}
    Ops[3] b = {.fns = {double, triple, quad}, .bias = {0, 1, 2}, .pos = 0}
    Ops[3] c = a + b   // bias becomes {1, 2, 3}
    i32 sum = 0
    for i32 v in c { sum = sum + v }
    // fns called with n=3: double(3)=6, triple(3)=9, quad(3)=12
    // + bias {1,2,3} -> 7 + 11 + 15 = 33
    return sum
}
