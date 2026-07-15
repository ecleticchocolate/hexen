//@ expect err unexpected 'if' in expression
// `if` is a statement, not an expression -- match already worked this way (a
// bare parse error, "unexpected 'match' in expression", since match is never
// reachable from parse_primary). if's codegen never had real result-handling:
// both arms happened to leave their last value in rax, a register-allocator
// coincidence for scalars that this rejects outright now, uniformly, rather
// than working for scalars and failing differently (a typecheck "cannot
// infer" error) for aggregates/enums/generics.
fn main() i32 {
    i32 c = 1
    i32 x = if c > 0 { 42 } else { 0 }
    return x
}
