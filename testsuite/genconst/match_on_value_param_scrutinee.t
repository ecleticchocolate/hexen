//@ expect val 100
// A const (value) generic param used directly as a `match` scrutinee. match
// resolves its scrutinee's type at PARSE time, where a value param N has no
// symbol yet -- it used to fail "undeclared identifier N". The parser knows N's
// pinned type (u32), so match takes it directly; N folds to its concrete value
// at instantiation, exactly as `if N == 0` already does.
fn f[u32 N]() i32 {
    match N {
        0 { return 100 }
        else { return 999 }
    }
}
fn main() i32 { return f[0]() }
