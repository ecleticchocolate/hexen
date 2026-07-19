//@ expect val 47
// Regression guard for the match_on_hkt_slot.t fix itself: the FIRST attempt
// at that fix (gating Type_Substitute's "Box* self param" auto-completion on
// sd->type_params == params, pointer identity with the current frame) broke
// this exact shape -- an ordinary generic impl method's implicit `self`
// param. self's type is built fresh per method (parser.c, "Inject self as
// first param") as a bare TYPE_STRUCT with no array-identity relationship to
// whatever frame later substitutes it, so identity-based gating rejected the
// legitimate case too. The real fix (Type.struct_unapplied, set ONLY by the
// deliberate-unapplied-argument call site) leaves this path untouched: self's
// type was never marked struct_unapplied, so it still auto-completes exactly
// as before.
struct Box[T] { T val }
impl Box[T] {
    fn get() T { return self.val }
    fn map[U](U v) U { return v }
}
fn main() i32 {
    Box[i32] b = {.val = 42}
    i32 x = b.map(5)
    return b.get() + x
}
