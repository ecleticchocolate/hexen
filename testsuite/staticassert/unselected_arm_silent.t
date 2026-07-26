//@ expect val 0
// An assert inside a `match T` arm that was NOT selected must not fire. This is
// why the check runs at typecheck rather than at parse time -- arms are chosen
// during typecheck, and a parse-time fold errored on every arm regardless.
fn probe[T]() void {
    match T {
        i32 { static_assert(sizeof(T) == 999, "i32 arm should not fire for u8") }
        else { }
    }
}
fn main() i32 { probe[u8]()  return 0 }
