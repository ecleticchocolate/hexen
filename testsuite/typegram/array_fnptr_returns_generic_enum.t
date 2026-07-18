//@ expect val 42
enum Option[T] { T Some  None }
fn mk(u32 x) Option[u32] { if x > 0 { return .Some(x) }  return .None }
fn main() i32 {
    (fn(u32) Option[u32])[1] fns
    fns[0] = mk
    Option[u32] r = fns[0](42)
    match r { .Some(v) { return (i32) v }  .None { return 0 } }
    return 0
}
