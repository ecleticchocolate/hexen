//@ expect val 304
fn main() i32 {
    match (fn() struct{i32 a})[3] {
        F[N] {
            match F {
                fn() R { match R { struct{ X a } { return (i32)N*100 + (i32)sizeof(X) } else { return -1 } } }
                else { return -2 }
            }
        }
        else { return -3 }
    }
}
