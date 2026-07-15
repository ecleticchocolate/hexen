//@ expect stdout
//@ | f32[2][5] val=15.000000 depth=2
//@ | f64[3]    val=6.750000 depth=1
//@ | f32[2][3][4] val=36.000000 depth=3
extern fn printf(u8* fmt, ...) i32
fn scale[T]() f64 {
    match T {
        f32  { return 1.5 }
        f64  { return 2.25 }
        E[N] { return (f64)N * scale[E]() }
        else { return 0.0 }
    }
}
fn depth[T]() u32 {
    match T {
        E[N] { return 1 + depth[E]() }
        else { return 0 }
    }
}
fn main() i32 {
    printf("f32[2][5] val=%f depth=%d\n", scale[f32[2][5]](), depth[f32[2][5]]())
    printf("f64[3]    val=%f depth=%d\n", scale[f64[3]](),    depth[f64[3]]())
    printf("f32[2][3][4] val=%f depth=%d\n", scale[f32[2][3][4]](), depth[f32[2][3][4]]())
    return 0
}
