//@ expect val 0
struct Vector {
    i32 x
    i32 y
}

impl Vector {
    fn __add(Vector other) Vector {
        return {.x = self.x + other.x, .y = self.y + other.y}
    }
}

fn main() i32 {
    Vector a = {.x = 1, .y = 2}
    Vector b = {.x = 3, .y = 4}
    Vector c = a + b
    if (c.x != 4) { return 1 }
    if (c.y != 6) { return 2 }
    return 0
}
