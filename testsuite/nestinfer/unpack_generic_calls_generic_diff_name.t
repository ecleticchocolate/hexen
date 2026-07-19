//@ expect val 77
// `unpack` initialized from a generic call whose callee uses a DIFFERENT type-param
// name. `unpack` typechecks its initializer eagerly at parse time, while
// outer_diff's T is still abstract, so inner(v) must bind its own U to the
// enclosing T. Regressed as "cannot infer generic type arguments for call to
// 'inner'" until unify_types learned to bind a callee param to an enclosing one.
fn inner[U](U v) U { return v }
fn outer_diff[T](T v) T { unpack x = inner(v); return x }
fn main() i32 { return outer_diff(77) }
