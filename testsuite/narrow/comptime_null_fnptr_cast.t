//@ expect val 1
// Regression: `(fn(...) T) null` -- casting the null pointer literal to a
// function-pointer type, the idiomatic "no dispatch function" sentinel used
// by REFERENCE.md's own existential-vtable showcase's fallback arm -- failed
// to fold inside `const {}`. ce_eval_cast's TYPE_FUNCTION guard only ever
// accepted two shapes as "safe, not an arbitrary unmapped address": a real
// function identifier, or a LIT_FN_SYMBOL-tagged literal (from a fn-typed
// const-generic param already bound to a concrete function). The folded
// operand being exactly 0 is a THIRD safe case for a different reason --
// it's never dereferenced as code, same as an ordinary runtime `fn(...) T f
// = null` is fine until called (and callers are expected to check first,
// same as this showcase's own `if dyns[k].obj != null` guard). Missing this
// case meant a real, working runtime construction (see gemini/tensor2.t's
// "Structural VTable Generation" section) could never be folded a second
// time inside a `const` -- the runtime and comptime paths diverged on the
// exact same code.
fn test() i32 {
    fn(void*) i32 f = (fn(void*) i32) null
    if f == null { return 1 }
    return 0
}
fn main() i32 {
    i32 res = 0
    const { res = test() }
    return res
}
