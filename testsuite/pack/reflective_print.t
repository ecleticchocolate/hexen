//@ expect stdout
//@ | Point: Point { x: 10, y: 20 }
//@ | Player: Player { name: Alice, pos: Point { x: 5, y: 15 }, scores: [100, 95, 88], ptr: null }
//@ | Array of Points: [Point { x: 1, y: 2 }, Point { x: 3, y: 4 }]
//@ | Color: Color
//@ | Custom: <Custom 42>
extern fn printf(u8* fmt, ...) i32
extern fn putchar(i32 c) i32

fn print_struct_fields[Orig, Walk, u32 N](Orig* obj) void {
    match Walk {
        struct { H; Rest... } {
            if N > 0 { printf(", ") }
            printf("%s: ", nameof(Orig, N))
            u8* base = (u8*)obj
            u64 off = offsetof(Orig, N)
            H* field_ptr = (H*)(base + off)
            print_val[H](*field_ptr)
            print_struct_fields[Orig, Rest, N + 1](obj)
        }
        struct {} {}
    }
}

fn print_val[T](T val) void {
    match T {
        impl { fn __print() } {
            val.__print()
        }
        u8*  {
            if (u8*)val == null { printf("null") }
            else { printf("%s", val) }
        }
        i32  { printf("%d", val) }
        u32  { printf("%u", val) }
        i64  { printf("%lld", val) }
        u64  { printf("%llu", val) }
        f64  { printf("%g", val) }
        f32  { printf("%g", (f64)val) }
        bool {
            if val { printf("true") }
            else   { printf("false") }
        }
        u8   { printf("%u", (u32)val) }

        // Function Pointers: fn(Args...) R
        fn(Args...) R {
            if (u64)val == 0 { printf("<fn null>") }
            else { printf("<fn 0x%llx>", (u64)val) }
        }

        // Generic Pointer: P*
        P* {
            if (void*)val == null { printf("null") }
            else { printf("0x%llx", (u64)val) }
        }

        // Generic Array: Elem[N]
        Elem[N] {
            printf("[")
            for u32 i = 0 to N {
                if i > 0 { printf(", ") }
                print_val[Elem](val[i])
            }
            printf("]")
        }

        // Generic Struct Shape
        struct { Any... } {
            printf("%s { ", nameof(T))
            print_struct_fields[T, T, 0](&val)
            printf(" }")
        }

        // Generic Enum Shape
        enum { Any... } {
            printf("%s", nameof(T))
        }

        else {
            printf("<??>")
        }
    }
}

fn print_impl[T](u8* fmt, T args) void {
    i32 i = 0
    while fmt[i] != 0 {
        if fmt[i] == '%' {
            if fmt[i + 1] == '%' {
                putchar('%')
                i = i + 2
                continue
            }
            match T {
                struct { H; Rest... } {
                    print_val(args._0)
                    print_impl(fmt + i + 1, (Rest)args)
                    return
                }
                struct {} {
                    printf("[MISSING]")
                    i = i + 1
                    continue
                }
            }
        } else {
            putchar((i32)fmt[i])
            i = i + 1
        }
    }
}

fn println[T](u8* fmt, T... args) void {
    print_impl(fmt, args)
    putchar('\n')
}

struct Point {
    i32 x
    i32 y
}

struct Player {
    u8* name
    Point pos
    i32[3] scores
    Point* ptr
}

enum Color { Red  Green  Blue }

struct CustomType { i32 id }
impl CustomType {
    fn __print() void {
        printf("<Custom %d>", self.id)
    }
}

fn dummy(i32 a, f64 b) bool { return true }

fn main() i32 {
    Point pt = {.x = 10, .y = 20}
    println("Point: %", pt)

    Player pl = {
        .name = "Alice",
        .pos = {.x = 5, .y = 15},
        .scores = {100, 95, 88},
        .ptr = null
    }
    println("Player: %", pl)

    Point[2] pts = { {.x = 1, .y = 2}, {.x = 3, .y = 4} }
    println("Array of Points: %", pts)

    Color col = .Red
    println("Color: %", col)

    CustomType ct = {.id = 42}
    println("Custom: %", ct)

    return 0
}
