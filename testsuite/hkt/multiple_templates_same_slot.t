//@ expect val 10
// Three different templates -- a 1-field struct, an enum, a 2-field struct --
// all bound to the same HKT slot M in the same program. Proves the mechanism
// isn't special-cased to any one template's shape; M is a genuine value, not
// a name the compiler happens to recognize.
struct Box[T] { T val }
enum Option[T] { T Some  None }
struct Pair[T] { T a  T b }

struct HKT[M, T] { M[T] data }

fn main() i32 {
    HKT[Box, i32] h1
    HKT[Option, i32] h2
    HKT[Pair, i32] h3
    h1.data = { .val = 1 }
    h2.data = { .Some = 2 }
    h3.data = { .a = 3, .b = 4 }
    i32 r2 = 0
    match h2.data { {.Some=v} { r2 = v }  else { r2 = -1 } }
    return h1.data.val + r2 + h3.data.a + h3.data.b
}
