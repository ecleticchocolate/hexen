//@ expect val 9
// A `match` whose scrutinee is an aggregate-returning generic method, where that
// method reads an AGGREGATE const-generic value-param (Shape2 S). The value-param
// is materialized as a global ($cgen$S$0) with its bytes in the static image.
// A codegen bug zero-initialized that global at runtime (the aggregate decl-init
// path didn't skip SYM_GLOBAL the way the scalar path did), so S.cols read 0 --
// only when a `match` on an aggregate result put an aggregate temp decl on that
// path.
// get_computed(1,1) = data[1*S.cols + 1]. S.cols==2 -> data[3] = .Some(9).
// The bug (S.cols==0) -> data[1] = .None, taking the wrong arm.
enum Option[T] { T Some  None }
struct Shape2 { u32 rows  u32 cols }
struct Tensor[T, Shape2 S] { T[4] data }
impl Tensor[T, Shape2 S] {
    fn get_computed(u32 r, u32 c) T { return self.data[r * S.cols + c] }
}
fn main() i32 {
    Tensor[Option[i32], {.rows=2, .cols=2}] m = {.data = {.Some(1), .None, .None, .Some(9)}}
    match m.get_computed(1, 1) {
        .Some(v) { return v }   // correct: S.cols==2 -> data[3] = .Some(9) -> 9
        .None    { return -1 }  // the bug: S.cols==0 -> data[1] = .None
    }
}
