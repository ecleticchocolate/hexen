//@ expect val 999
struct Point {
    i32 x
    i32 y
}

// Function default arguments with struct literal and array literal:
fn process(Point p = {.x = 100, .y = 200}, i32[3] arr = {10, 20, 30}) i32 {
    return p.x + p.y + arr[0] + arr[1] + arr[2]
}

fn main() i32 {
    // Calling process() with omitted args triggers default Point {100, 200} and default array {10, 20, 30}:
    // 100 + 200 + 10 + 20 + 30 = 360
    i32 v = process()

    // 360 + 639 = 999
    return v + 639
}
