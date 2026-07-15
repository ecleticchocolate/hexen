//@ expect val 7
fn f(void* p) void { }
fn main() i32 {
    fn(void*) g = f          // omitted return must unify with explicit void
    fn(void*) void h = f     // and explicit with explicit
    return 7
}
