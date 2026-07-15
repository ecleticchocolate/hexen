//@ expect val 42
// An alias is interchangeable with what it names (specs.md §24), and `impl` is a
// type site. It used to grab the raw token text instead of resolving the alias, so
// `self` was built as a struct nobody had declared -- surfacing later as "field
// access on a non-struct value". Both spellings must reach the same method.
struct Raw { u32 n }
alias RA = Raw
impl RA { fn dbl() u32 { return self.n * 2 } }
fn main() u32 {
    RA  a = {.n = 21}
    Raw b = {.n = 21}
    if a.dbl() != b.dbl() { return 0 }
    return a.dbl()
}
