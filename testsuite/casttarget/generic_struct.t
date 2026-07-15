//@ expect val 6
struct List[T] { T a  T b  T c }
fn sum(List[u32] l) u32 { return l.a + l.b + l.c }
fn main() i32 { return (i32) sum((List[u32]){.a = 1, .b = 2, .c = 3}) }
