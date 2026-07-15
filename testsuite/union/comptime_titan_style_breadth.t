//@ expect val 1352807316
// Titan-style breadth test for unions under comptime (mirrors titan/D's
// impl-mutation-through-self shape, but for union storage): mutation
// through self in an impl method, mixed-width overlap read, IEEE-754 type
// punning, and an array of unions, all folded by ConstEval and required to
// match the runtime JIT path bit-for-bit.
union Slot { i32 as_int  f32 as_float  u8 as_byte }
struct Cell { Slot slot }
impl Cell {
    fn set_int(i32 v) { self.slot.as_int = v }
    fn get_int() i32 { return self.slot.as_int }
    fn get_low_byte() i32 { return (i32) self.slot.as_byte }
}
union Pun { i32 i  f32 f }

fn compute() i32 {
    i32 total = 0

    Cell c
    c.set_int(0x11223344)
    total = total + c.get_int()             // 0x11223344

    total = total + c.get_low_byte()        // + 0x44 (68)

    Pun p
    p.f = 1.0
    total = total + p.i                     // + IEEE-754 bits of 1.0f

    Slot[2] arr
    arr[0].as_int = 5
    arr[1].as_int = 7
    total = total + arr[0].as_int + arr[1].as_int   // + 5 + 7

    return total
}

const i32 TOTAL = compute()
fn main() i32 { return TOTAL }
