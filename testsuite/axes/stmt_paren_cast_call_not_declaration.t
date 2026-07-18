//@ expect stdout
//@ | called with 5
//@ | called with 10
//@ | r=11
// Regression: `(TYPE) name(args)` as a bare STATEMENT (e.g. discarding a
// call's result via a cast: `(void) get(5)`) was silently misparsed as a
// fresh declaration `TYPE name` -- parse_type() happily accepts the
// parenthesized `(TYPE)` as a type spelling on its own, so the statement
// parser committed to "this is a declaration" before ever noticing the `(`
// that followed the name. The call's arguments were then read as a
// SEPARATE, unrelated parenthesized expression, and the call itself never
// ran. Worse: if `name` was an existing FUNCTION, this silently declared a
// new local of that name, SHADOWING the function for the rest of the scope
// -- so a later, legitimate call to the same function failed with a
// misleading "calling non-function" error pointing at the wrong line.
//
// Fix: after parse_type() succeeds and an identifier follows, peek one more
// token. A real declaration's name is never immediately followed by '(' (no
// "declare and call in one statement" grammar exists) -- so seeing '(' means
// the whole `(TYPE) name(...)` was a cast applied to a call result, not a
// declaration. Roll back to before parse_type() ran and re-parse as an
// ordinary expression statement instead.
extern fn printf(u8* fmt, ...) i32
fn get(i32 x) i32 { printf("called with %d\n", x)  return x + 1 }
fn main() i32 {
    (void)get(5)              // discarded call: must actually run get(5)
    i32 r = get(10)           // `get` must still resolve to the FUNCTION here,
    printf("r=%d\n", r)       // not a shadowing local left behind by the line above
    return 0
}
