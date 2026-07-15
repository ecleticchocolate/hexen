//@ expect val 43
fn store[T](T x) i32 {
    match T {
        E[N] {
            E* p = new E
            *p = x[0]
            i32 r = (i32)(*p) + (i32)N
            delete p
            return r
        }
        else { return -1 }
    }
}
fn main() i32 {
    i32[3] a
    a[0] = 40
    return store(a)
}
