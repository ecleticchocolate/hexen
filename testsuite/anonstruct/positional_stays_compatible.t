//@ expect val 7
// A POSITIONAL anon struct (no field names -- the tuple shape the test corpus
// uses everywhere, and what `T... args` synthesizes) still flows into a
// named-field target. That relaxation is what REFERENCE.md's hand-built
// existential showcase depends on, and restricting the nominal rule to
// "both sides name their fields" keeps it working.
fn takes_named(struct{i32 a  i32 b} p) i32 { return p.a }
fn main() i32 {
    struct{i32; i32} tup
    tup._0 = 7
    return takes_named(tup)
}
