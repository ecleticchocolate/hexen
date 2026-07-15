//@ expect val 100
// A no-payload enum variant's payload type is stored as NULL (see parser.c:
// "f->type = ftype // NULL for a no-payload variant"). Bound to a wildcard
// (`enum { H h  Rest... r }`), H correctly carries that NULL through --
// nameof(H) already printed "void" for it -- but comparing it against a literal
// `void` pattern inside a NESTED generic match silently always failed, even
// though the two are meant to be the same concept.
//
// Root cause (two identical copies of one bug, in Resolve_Reflect_Matches and
// Typecheck_Tree's AST_IF case): both treated ANY NULL reflect_scrutinee as
// "still generic, substitution hasn't run yet" and left the match completely
// unresolved -- conflating "NULL because not substituted" with "NULL because
// the resolved value genuinely IS void." Fixed by only deferring when the
// scrutinee is an ACTUAL unresolved TYPE_PARAM node (which requires it to be
// non-NULL), not whenever it's NULL. Also needed reflect_unify itself to know
// NULL and an explicit `void` pattern denote the same thing (Type_IsVoidLike,
// mirroring the existing fn-return-type void normalization in Type_Equals).
enum ShapeDirect { None  i32 Circle }

fn inner_probe[H]() i32 {
    match H {
        void { return 100 }
        else { return 200 }
    }
}

fn outer_probe[Walk]() i32 {
    match Walk {
        enum { H h  Rest... r } { return inner_probe[H]() }
        enum {} { return 999 }
    }
}

fn main() i32 { return outer_probe[ShapeDirect]() }
