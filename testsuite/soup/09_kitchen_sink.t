//@ expect stdout
//@ | circle(1,2) poly(10,20,30) id=7
extern fn printf(u8* fmt, ...) i32;

struct Point { i32 x; i32 y; }
enum Shape[T, u32 N] {
    Point Circle
    T[N] Poly
    None
}

struct Scene[T, u32 N] {
    Shape[T, N] main_shape;
    Shape[T, N][2] extras;
    u32 id;
}

fn main() i32 {
    Scene[i32, 3] s = {
        .main_shape = .Circle({ .x = 1, .y = 2 }),
        .extras = { .Poly({10,20,30}), .None },
        .id = 7
    }

    match s {
        {
            .main_shape = .Circle({ .x = cx, .y = cy }),
            .extras = { .Poly({p0, p1, p2}), .None },
            .id = i
        } {
            printf("circle(%d,%d) poly(%d,%d,%d) id=%u\n", cx, cy, p0, p1, p2, i)
        }
        else { printf("no match\n") }
    }
    return 0
}
