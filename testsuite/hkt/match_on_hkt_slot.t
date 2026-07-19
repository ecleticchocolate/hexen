//@ expect val 1
// match T on the HKT slot itself (M), not just on an ordinary type param
// nested inside it -- distinguishing "which template is M bound to" at
// compile time. This is a genuinely separate mechanism from field storage:
// reflect_unify (reflections.c) has no idea Fix A/B exist, so a bare,
// deliberately-unapplied `TYPE_STRUCT` (M bound to plain `Box`, no args)
// flowing into Type_Substitute's older `TYPE_STRUCT` case (the pre-existing
// "Box* self param" branch, which auto-completes a template using whatever
// same-named param happens to be in the CURRENT frame) used to get
// force-instantiated by NAME COINCIDENCE -- e.g. describe[M,T]'s own
// unrelated "T" colliding with Box's declared param name "T", silently
// turning the match pattern `Box` into `Box[i32]` while the real scrutinee
// stayed bare `Box`, so the match always fell through to `else` even for the
// exactly-correct case. Fixed via Type.struct_unapplied, set only by the
// "let an unapplied template through" call site and checked by that older
// branch to opt bare Fix-A nodes out of auto-completion.
struct Box[T] { T val }
struct Bag[T] { T val }
struct HKT[M, T] { M[T] data }

fn describe[M, T](HKT[M, T] h) i32 {
    match M {
        Box  { return 1 }
        else { return 0 }
    }
}

fn main() i32 {
    HKT[Box, i32] h1
    HKT[Bag, i32] h2
    i32 r1 = describe(h1)   // 1 -- M is genuinely Box
    i32 r2 = describe(h2)   // 0 -- M is genuinely Bag, must NOT match
    return r1 - r2          // 1 - 0 = 1
}
