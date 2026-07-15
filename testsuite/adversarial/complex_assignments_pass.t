//@ expect val 0
extern fn printf(u8* fmt, ...) i32

struct Box[T] { T val }
struct Pair[U, V] { U first  V second }
struct Another[V, F] { V tag  F handler }
struct Box2[U, A] { U id  A inner }

const u32 SIZE = 3
const u32 A_SIZE = 2
const u32 B_SIZE = 1 + 1

fn concrete_fn(i32 a, u32 b) f32 { return (f32)a + (f32)b }
fn add_u32(u32 a, u32 b) u32 { return a + b }
fn mul_f64(f64 a, f64 b) f64 { return a * b }
fn h_u32_f64(i32 a, u32 b) f64 { return (f64)a * (f64)b }
fn h_u32_u8(i32 a, u32 b) u8 { return (u8)(a + (i32)b) }

fn make_box[T](T v) Box[T] { Box[T] b; b = {.val = v}; return b }
fn copy_arr[T](T[4] src) T[4] { T[4] dst; dst = src; return dst }
fn make_box_arr[T](T a, T b) Box[T][2] { Box[T][2] arr; arr[0] = {.val = a}; arr[1] = {.val = b}; return arr }
fn sum_box_arr[T](Box[T][2] arr) T { return arr[0].val + arr[1].val }

fn make_grid[T](T fill) T[2][3] {
    T[2][3] g
    for u32 i = 0 to 2 { for u32 j = 0 to 3 { g[i][j] = fill } }
    return g
}

fn make2[U, V](U uval, V vtag, fn(i32, U) V h) Box2[U, Another[V, fn(i32, U) V]] {
    Another[V, fn(i32, U) V] inner = {.tag = vtag, .handler = h}
    Box2[U, Another[V, fn(i32, U) V]] b = {.id = uval, .inner = inner}
    return b
}

fn main() i32 {
    u32[4] a1 = {1, 2, 3, 4}
    u32[4] b1 = {0, 0, 0, 0}
    b1 = a1
    Box[u32] x = make_box(42)
    Box[u32][2] arr
    arr[0] = make_box(1)
    arr[1] = make_box(2)
    u32[4] cb = copy_arr(a1)
    f64[4] fa = {1.5, 2.5, 3.5, 4.5}
    f64[4] fcb = copy_arr(fa)
    Box[u32][2] ub = make_box_arr(3, 4)
    u32 s1 = sum_box_arr(ub)
    Pair[u32, f64][SIZE] parr
    for u32 i = 0 to SIZE { parr[i] = {.first = i, .second = (f64)i * 1.5} }
    u32[A_SIZE] ax = {10, 20}
    u32[B_SIZE] ay
    ay = ax
    u32[2][3] grid = make_grid(7)
    u32[2][3] grid2
    grid2 = grid
    Another[u8, fn(i32, u32) f32] inner9 = {.tag = 1, .handler = concrete_fn}
    Box[Another[u8, fn(i32, u32) f32]] outer9 = {.val = inner9}
    f32 r9 = outer9.val.handler(10, 20)
    Box2[u32, Another[u8, fn(u32, u32) u32]] bu = {.id = 5, .inner = {.tag = 0, .handler = add_u32}}
    u32 r10a = bu.inner.handler(3, 4)
    Box2[f64, Another[u8, fn(f64, f64) f64]] bf = {.id = 2.0, .inner = {.tag = 0, .handler = mul_f64}}
    f64 r10b = bf.inner.handler(3.0, 4.0)
    Box2[u32, Another[f64, fn(i32, u32) f64]] m1 = make2(7, 0.0, h_u32_f64)
    f64 r11a = m1.inner.handler(3, m1.id)
    Box2[u32, Another[u8, fn(i32, u32) u8]] m2 = make2(7, 0, h_u32_u8)
    u8 r11b = m2.inner.handler(3, m2.id)
    return 0
}
