//@ expect stdout
//@ | even(4) = 1
//@ | even(3) = 0
// Two mutually recursive generics whose type parameter is PHANTOM -- T appears in
// neither the arguments nor the return type, so it cannot be inferred and the explicit
// brackets are mandatory. A cycle forces one direction to forward-reference, so this
// was previously unwritable at all.
extern fn printf(u8* fmt, ...) i32
fn even[T](u32 n) u32 {
    if n == 0 { return 1 }
    return odd[T](n - 1)
}
fn odd[T](u32 n) u32 {
    if n == 0 { return 0 }
    return even[T](n - 1)
}
fn main() i32 {
    printf("even(4) = %d\n", even[i32](4))
    printf("even(3) = %d\n", even[i32](3))
    return 0
}
