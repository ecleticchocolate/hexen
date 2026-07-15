//@ expect val 20
struct Router[T, u32 N] {
    (fn(T) T)[N] routes
}

fn add_one(i32 x) i32 { return x + 1 }
fn sub_one(i32 x) i32 { return x - 1 }

fn main() i32 {
    Router[i32, 2] r
    r.routes[0] = add_one
    r.routes[1] = sub_one
    
    return r.routes[0](10) + r.routes[1](10) // 11 + 9
}
