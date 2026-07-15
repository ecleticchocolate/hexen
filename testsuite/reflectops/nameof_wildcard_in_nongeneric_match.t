//@ expect stdout
//@ | A=i32 B=i32
// nameof(A)/nameof(B) on a wildcard captured by `match T` inside an ORDINARY,
// non-generic function -- previously failed with "never resolved to a
// constant" while sizeof(A) on the IDENTICAL binding worked fine.
//
// Root cause: a captured wildcard's binding gets folded into the arm body via
// clone_ast, but clone_ast's own AST_NAMEOF case only substitutes the type
// reference -- it never actually converts the node to a real AST_STRING. That
// conversion lives solely in Resolve_Reflect_Matches, which Generic_Instantiate
// already calls right after its own clone_ast call for a GENERIC function's
// body. But Typecheck_Tree's AST_IF case is the OTHER place a `match T` gets
// resolved -- for an ordinary, non-generic function -- and it called clone_ast
// without ever following up with Resolve_Reflect_Matches, leaving nameof
// permanently unresolved in that one path. offsetof has the identical shape
// and would have the same gap; sizeof doesn't, since AST_SIZEOF needs no
// fold-to-string step at all.
extern fn printf(u8* fmt, ...) i32
fn probe() i32 {
    match fn(i32) i32 {
        fn(A) B {
            printf("A=%s B=%s\n", nameof(A), nameof(B))
            return 1
        }
        else { return 0 }
    }
}
fn main() i32 { return probe() }
