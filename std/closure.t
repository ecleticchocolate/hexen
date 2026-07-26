// Fn[T, R] - a callable bundled with its captured values.
//
// No type erasure: T is the anonymous struct that `T... args` already
// synthesizes, so a closure's captures stay a real, inspectable type. That is
// what makes `match T { struct { A; B } { ... } }` over a closure's captures
// possible at all -- an erased closure has nothing left to match on.
//
// No allocation either. The captures live inline in the Fn value, so a closure
// is exactly as big as what it captured and dies with its scope like any other
// local. There is no __delete() here because Fn owns nothing: it holds a
// function pointer and a bundle whose own fields are destroyed, if they need
// destroying, by the same scope-exit RAII that destroys any struct.
//
// CAPTURE SEMANTICS: the bundle is a byte copy of the arguments, the same rule
// assignment uses everywhere. For a capture that owns a resource (a Vector,
// say) that copy ALIASES the original rather than duplicating it -- so the
// closure must not outlive what it captured, and the captured value must not be
// destroyed while the closure is still callable. Capturing a borrowed view (a
// pointer, an index, a scalar) rather than an owning value avoids the question
// entirely, and is the shape to reach for.
// The capture bundle, spelled positionally. `Fn[Args[i32, i32], i32]` rather
// than `Fn[struct{i32; i32}, i32]` -- same type, readable name.
pub alias Args[Ts...] = Ts

pub struct Fn[T, R] {
    fn(T) R body
    T captures
}

pub impl Fn[T, R] {
    // `f()` -- run the body against the captured bundle.
    fn __call() R {
        return self.body(self.captures)
    }
}

// Bundle `body` with the trailing arguments it should later be called on:
//
//     auto f = closure(add, 10, 20)
//     i32 s = f()
//
// `T... args` collects the captures into one anonymous struct and T is that
// struct's type, so the returned Fn carries its capture layout in its own type
// -- nothing boxed, nothing erased, and `sizeof` tells the truth about it.
pub fn closure[T, R](fn(T) R body, T... args) Fn[T, R] {
    // Reject an owning capture AT COMPILE TIME, one capture at a time.
    //
    // The bundle is a byte copy, so capturing a Vector (or anything else with a
    // destructor) by value leaves the closure and the original both pointing at
    // one buffer, and both destroy it -- a double free from five lines of
    // ordinary-looking code. Capturing `&v` instead is the correct spelling and
    // costs one character; the point of this guard is that forgetting it is a
    // compile error naming the fix, not a crash at runtime.
    //
    // The `P*` arm must come FIRST: `Owning` answers yes for a POINTER to an
    // owning type as well (the capability query auto-derefs), so without the
    // pointer arm ahead of it the correct `&v` spelling would be rejected too.
    //
    // static_assert rather than the older "return an identifier that does not
    // exist" trick: that reported inside this file instead of at the call site,
    // and a user who declared a global with the sentinel's name silently turned
    // the check off (verified -- it compiled and then double-freed).
    guard_captures[T]()
    return { .body = body, .captures = args }
}

// Walks the capture bundle field by field. Not folded into closure() itself
// because peeling a pack tail needs to recurse on the REST type, which a
// function can only do by calling itself.
fn guard_captures[T]() void {
    match T {
        struct {} { }
        struct { H; Rest... } {
            match H {
                P*     { }
                Owning {
                    static_assert(0, "closure cannot capture an owning value by value -- the capture bundle is a byte copy, so it would alias the original's resource and both would destroy it. Pass a pointer instead: closure(f, &v)")
                }
                else   { }
            }
            guard_captures[Rest]()
        }
        else { }
    }
}
