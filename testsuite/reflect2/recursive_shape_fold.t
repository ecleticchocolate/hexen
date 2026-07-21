//@ expect stdout
//@ | recursive shape cost: 18
extern fn printf(u8* fmt, ...) i32
fn shape_cost[T]() u32 {
    match T {
        i32  { return 4 }
        u8   { return 1 }
        P*   { return 8 }
        E[N] { return N * shape_cost[E]() }
        struct { H; Rest... } {
            return shape_cost[H]() + shape_cost[Rest]()
        }
        struct {  } { return 0 }
        else { return 0 }
    }
}
fn main() i32 {
    printf("recursive shape cost: %d\n",
        shape_cost[struct{ i32 a  u8[4] b  i32* c  struct{ u8 x  u8 y } d }]())
    return 0
}
