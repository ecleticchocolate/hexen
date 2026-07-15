//@ expect val 42
enum Opt[T] { T Some  None }
fn some[T](T x) Opt[T] { return .Some{x} }
fn unwrap[T](Opt[T] o, T d) T { match o { .Some{v} { return v }  .None { return d } } }
fn map[T](Opt[T] o, fn(T) T f) Opt[T] { match o { .Some{v} { return .Some{f(v)} }  .None { return .None } } }
fn inc(i32 x) i32 { return x + 1 }
fn main() i32 {
    Opt[i32] o = some(40)
    o = map(o, inc)
    o = map(o, inc)
    Opt[i32] n = .None
    n = map(n, inc)
    return unwrap(o, 0) + unwrap(n, 0)
}
