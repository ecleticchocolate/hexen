//@ expect val 15
// Regression: a generic's const-value param can be pinned to a type that
// itself references an EARLIER param in the same list (`Container[T, Box[T]
// val]` -- the second param's pin is `Box[T]`, naming the first). Resolving
// an inline brace-literal argument for that slot (`Container[i32, {.val=5}]`)
// requires substituting the already-parsed earlier args into the pin type
// BEFORE resolving the literal, or the literal typechecks against the
// struct's own still-abstract declared type (`T`) and always fails
// ("cannot assign i32 to T"). This substitution was missing at THREE
// independently hand-written call sites that each parse a generic argument
// list (struct instantiation, explicit generic function calls, and generic
// aliases) -- found via one, "fixed" it, then found the identical bug
// unfixed at a second site, which is exactly the "same primitive needed at
// multiple sites, not actually shared" pattern this session kept finding
// elsewhere. All three now share one parse_generic_arg_list function.
struct Box[T] { T val }

struct Container[T, Box[T] val] { i32 tag }

fn make[T, Box[T] val]() i32 { return val.val }

alias Held[T, Box[T] val] = Box[T]

fn main() i32 {
    Container[i32, {.val=1}] c = {.tag = 2}
    i32 r1 = c.tag                     // 2

    i32 r2 = make[i32, {.val=5}]()     // 5

    Held[i32, {.val=8}] h = {.val=8}
    i32 r3 = h.val                     // 8

    return r1 + r2 + r3   // 2 + 5 + 8 = 15
}
