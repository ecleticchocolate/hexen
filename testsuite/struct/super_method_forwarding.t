//@ expect val 142
struct Base {
    i32 val
}

impl Base {
    fn add(i32 extra) i32 {
        return self.val + extra
    }
}

// Derived embeds super Base base:
struct Derived {
    super Base base
    i32 bonus
}

// Generic Traced decorator embeds super T payload:
struct Traced[T] {
    super T payload
    u32 timestamp
}

fn main() i32 {
    Derived d = {.val = 100, .bonus = 10}
    // d.add(30) automatically forwards to d.base.add(30) via super Base!
    i32 v1 = d.add(30) // 100 + 30 = 130

    Traced[Base] tb = {.payload = {.val = 10}, .timestamp = 5}
    // tb.add(2) automatically forwards to tb.payload.add(2) via super Base!
    i32 v2 = tb.add(2) // 10 + 2 = 12

    // 130 + 12 = 142
    return v1 + v2
}
