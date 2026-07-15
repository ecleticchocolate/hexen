//@ expect val 108
enum Value { f32 Float i32 Int }
struct Entry { Value val i32 id }
fn compute() i32 {
    Entry[4] table = {
        {.val = .Float{1.5}, .id = 1},
        {.val = .Int{10}, .id = 2},
        {.val = .Float{2.5}, .id = 3},
        {.val = .Int{20}, .id = 4}
    }
    i32 acc = 0
    for u32 i = 0 to 4 {
        match table[i].val {
            .Float{f} { acc = acc + (i32)(f * (f32)table[i].id) }
            .Int{n} { acc = acc + (n * table[i].id) }
        }
    }
    return acc
}
const i32 RES = compute()
fn main() i32 { return RES }
