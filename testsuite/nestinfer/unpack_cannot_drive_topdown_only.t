//@ expect err cannot infer generic type arguments for call to 'wrap'
// BOUNDARY (not a bug): `unpack` typechecks its initializer eagerly with NO target
// type, so a chain whose type args are only inferable TOP-DOWN cannot be unpack-bound
// -- make()'s T here can only come from an outer Box[i32], which `unpack` doesn't
// supply. `Box[i32] w = wrap(make(42))` (explicit target) is the supported form.
// This test guards the enclosing-param fix from over-reaching: it must bind a
// caller's OWN param, never a foreign call's unresolved return param. If this
// starts compiling, the fix has become too eager and the wrap(make()) tests are
// next to break.
struct Box[T] { T value }
fn make[T](u32 seed) T { return (T)seed }
fn wrap[T](T v) Box[T] { return {.value = v} }
fn consume(Box[i32] b) i32 { return b.value }
fn main() i32 { unpack w = wrap(make(42)) return consume(w) }
