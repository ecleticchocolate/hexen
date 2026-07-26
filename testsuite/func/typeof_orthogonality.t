//@ expect val 1234
struct Point {
    i32 x
    i32 y
}

struct Container {
    Point pt
    typeof(100) count
}

fn calculate(typeof(100) a, typeof(200) b) typeof(a + b) {
    return a + b
}

fn main() i32 {
    // 1. Variable declaration with typeof:
    typeof(10 + 20) x = 30

    // 2. Struct field with typeof:
    Container c = {.pt = {.x = 100, .y = 200}, .count = 300}

    // 3. Function call with typeof parameter & return type:
    typeof(x) res = calculate(x, c.pt.x + c.pt.y + c.count) // 30 + 600 = 630

    // 4. Pointer and Array postfix composition on typeof:
    typeof(x)* ptr = &res
    typeof(x)[2] arr = {100, 531}

    // 5. Cast with typeof:
    typeof(x) casted = (typeof(x)) 3.14 // 3

    // 6. Pointer to struct using typeof(expr)*:
    Point p_var = {.x = 1000, .y = 2000}
    typeof(p_var)* p_allocated = &p_var

    // 630 (*ptr) + 3 (casted) + 100 (arr[0]) + 531 (arr[1]) + 1000 (p_allocated.x) - 1030 = 1234
    return *ptr + casted + arr[0] + arr[1] + p_allocated.x - 1030
}
