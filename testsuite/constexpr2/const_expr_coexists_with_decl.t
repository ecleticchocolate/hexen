//@ expect stdout
//@ | global decl : 100
//@ | local  decl : 42
//@ | float  decl : 2.500000
//@ | agg    decl : 7 8 9
//@ | expr   form : 50
extern fn printf(u8* fmt, ...) i32
const u32 GLOBAL_C = 100
fn main() i32 {
    const u32 LOCAL_C = 42
    const f64 FL = 2.5
    const u32[3] ARR = {7,8,9}
    printf("global decl : %d\n", GLOBAL_C)
    printf("local  decl : %d\n", LOCAL_C)
    printf("float  decl : %f\n", FL)
    printf("agg    decl : %d %d %d\n", ARR[0], ARR[1], ARR[2])
    u32 t1
    const { t1 = LOCAL_C + 8 }
    printf("expr   form : %d\n", t1)
    return 0
}
