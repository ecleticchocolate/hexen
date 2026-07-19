//@ expect val 100
// `match` on a scrutinee that mentions a const value-generic param -- bare N, and
// buried in a compound expression (N*2, arr[N]). A previous implementation resolved
// the scrutinee's type at parse time and failed these with "undeclared identifier N";
// the value engine defers classification to typecheck, so any value expression works.
fn f[u32 N](i32[4] a) i32 {
    match N * 2 {          // N=3 -> 6
        6 { return a[N] }  // arr indexed by a value param
        else { return 999 }
    }
}
fn main() i32 {
    i32[4] a = { 0, 0, 0, 100 }
    return f[3](a)         // N*2==6 -> return a[3] == 100
}
