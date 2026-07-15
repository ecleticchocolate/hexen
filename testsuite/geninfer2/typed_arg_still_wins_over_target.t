//@ expect err cannot assign
fn make_arr[T](T seed) T[3] {
    T[3] a
    a[0] = seed
    a[1] = seed
    a[2] = seed
    return a
}
fn main() u32 {
    i32 seed_val = 7
    u32[3] r = make_arr(seed_val)
    return r[0] + r[1] + r[2]
}
