//@ expect val 35
fn alloc[T](u32 n, T v) T* { T* a = new[n] T; for u32 i = 0 to n { a[i] = v } return a }
fn main() i32 {
    i32* a = alloc(5, 7)
    i32 s = 0; for u32 i = 0 to 5 { s = s + a[i] }
    delete a
    return s
}
