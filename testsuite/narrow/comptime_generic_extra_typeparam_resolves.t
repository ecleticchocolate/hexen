//@ expect val 10
// A method declaring its OWN extra type param beyond the struct's (identity[U],
// U separate from Box's own T) used to be unfoldable in a const context --
// ce_eval_call only ran infer_generic when NO type args were known yet, but a
// struct-receiver method already has self_type_args=[T] populated by
// try_rewrite_method_call, so the gate never fired and the call failed even
// though U was trivially inferrable from the argument. Fixed: infer_generic
// now runs whenever the known arg count doesn't already match the callee's
// full type-param count, and it seeds from self_type_args itself before
// inferring what's still missing.
struct Box[T] { T val }
impl Box[T] {
    fn identity[U](U x) U { return x }
}
fn build() u32 {
    Box[i32] b = {.val = 5}
    return b.identity(10)
}
const u32 R = build()
fn main() i32 { return (i32)R }
