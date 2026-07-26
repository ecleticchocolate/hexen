//@ expect val 8990
extern fn printf(u8* fmt, ...) i32

struct Point {
    i32 x
    i32 y
}

// -------------------------------------------------------------
// 1. Generic Functions with type-dependent default: sizeof(T)
// -------------------------------------------------------------
fn generic_default[T](T val, i32 sz = (i32)sizeof(T)) i32 {
    return sz
}

// -------------------------------------------------------------
// 2. Const Generics with value-dependent default: N * 10
// -------------------------------------------------------------
fn const_generic_default[u32 N](i32 multiplier = (i32)N * 10) i32 {
    return multiplier
}

// -------------------------------------------------------------
// 3. Method with default arguments + super forwarding
// -------------------------------------------------------------
struct Service {
    i32 port
}

impl Service {
    fn run(i32 target_port = 8000, i32 flags = 1) i32 {
        return self.port + target_port + flags
    }
}

struct ManagedService {
    super Service svc
    u32 id
}

// -------------------------------------------------------------
// 4. Value Pack function with defaults (T... rest)
// -------------------------------------------------------------
fn value_pack_default[T](i32 prefix = 100, T... rest) i32 {
    return prefix
}

fn main() i32 {
    // 1. Generic defaults:
    // generic_default[i32](42) -> sizeof(i32) = 4
    i32 g1 = generic_default[i32](42)
    // generic_default[Point]({10,20}) -> sizeof(Point) = 8
    Point p = {.x = 10, .y = 20}
    i32 g2 = generic_default[Point](p)

    // 2. Const generic defaults:
    // const_generic_default[5]() -> 5 * 10 = 50
    i32 cg1 = const_generic_default[5]()
    // const_generic_default[5](999) -> explicit arg 999
    i32 cg2 = const_generic_default[5](999)

    // 3. Method default + super forwarding:
    ManagedService ms = {.svc = {.port = 10}, .id = 1}
    // ms.run() -> forwards to ms.svc.run(8000, 1) -> 10 + 8000 + 1 = 8011
    i32 m1 = ms.run()
    // ms.run(5000) -> forwards to ms.svc.run(5000, 1) -> 10 + 5000 + 1 = 5011
    i32 m2 = ms.run(5000)

    // 4. Value Pack default:
    // value_pack_default[i32]() -> prefix = 100
    i32 vp1 = value_pack_default[i32]()

    // Sum calculation:
    // g1 = 4, g2 = 8, cg1 = 50, cg2 = 999
    // m1 = 8011, m2 = 5011, vp1 = 100
    // (4 + 8 + 50 + 999 + 8011 - 5011 + 100) = 4161
    // 4161 + 4829 = 8990
    i32 result = (g1 + g2 + cg1 + cg2 + m1 - m2 + vp1) + 4829
    return result
}
