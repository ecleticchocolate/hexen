//@ expect val 0
extern fn printf(u8* fmt, ...) i32;

struct Point {
    i32 x;
    i32 y;
}

fn make_point() Point {
    return { .x = 3, .y = 9 };
}

fn divmod(i32 a, i32 b) struct { i32 q  i32 r } {
    return { .q = a / b, .r = a % b };
}

fn main() i32 {
    unpack { .x = px, .y = py } = make_point();
    printf("named: px=%d py=%d\n", px, py);

    unpack { qx, qy } = make_point();
    printf("positional: qx=%d qy=%d\n", qx, qy);

    unpack whole = make_point();
    printf("whole bind: whole.x=%d whole.y=%d\n", whole.x, whole.y);

    unpack { q, r } = divmod(17, 5);
    printf("multi-return: q=%d r=%d\n", q, r);

    i32[3] arr = { 10, 55, 99 };
    unpack { a0, a1, a2 } = arr;
    printf("array: %d %d %d\n", a0, a1, a2);

    return 0;
}
