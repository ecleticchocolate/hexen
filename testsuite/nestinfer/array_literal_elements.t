//@ expect val 21
fn make_f32[T](u32 seed) T { return (T) 7.0 }
fn main() i32 { f32[3] arr = {make_f32(1), make_f32(2), make_f32(3)} return (i32)(arr[0]+arr[1]+arr[2]) }
