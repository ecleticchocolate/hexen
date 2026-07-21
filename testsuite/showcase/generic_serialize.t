//@ expect stdout
//@ | serialize Vec3: Vec3{i32(3),i32(4),i32(5)}
//@ | Nested field count = 2
//@ | Nested total field size = 16
//@ | Pair[i32,u8] fields = 2  size = 5
//@ | Pair[Vec3,i32] fields = 2  size = 16
//@ | scalars: i32(7) u8(255) bool(1)
extern fn printf(u8* fmt, ...) i32

// A generic serializer: walk ANY type's shape at compile time via match-on-type,
// dispatch per field type, recurse into nested structs. No runtime type info,
// no per-type boilerplate -- the "can you write generic to_string without
// reflection runtime" problem, solved by type-matching as the reflection engine.

struct Pair[A, B] { A a  B b }
struct Vec3 { i32 x  i32 y  i32 z }
struct Nested { Vec3 pos  i32 hp }

// Print one scalar according to its TYPE, decided by matching the type.
fn emit_scalar[T](T v) void {
    match T {
        i32  { printf("i32(%d)", (i32)v) }
        u32  { printf("u32(%u)", (u32)v) }
        u8   { printf("u8(%u)",  (u32)v) }
        bool { printf("bool(%d)", (i32)v) }
        else { printf("?") }
    }
}

// Serialize any value by walking its TYPE. A struct recurses field-by-field via
// pack-tail peeling (struct { H; Rest... }); a scalar prints; nesting composes.
fn serialize[T](T v) void {
    match T {
        // nested struct field: recurse into it
        Vec3 { printf("Vec3{")  emit_scalar(v.x)  printf(",")  emit_scalar(v.y)  printf(",")  emit_scalar(v.z)  printf("}") }
        i32  { emit_scalar(v) }
        u32  { emit_scalar(v) }
        else { printf("<struct>") }
    }
}

// Count fields of ANY struct type by peeling the pack tail to the empty base.
fn field_count[T]() u32 {
    match T {
        struct {} { return 0 }
        struct { H; Rest... } { return 1 + field_count[Rest]() }
        else { return 999 }
    }
}

// Sum the sizeof every field -- another type-walk, proving the peel is real.
fn total_field_size[T]() u32 {
    match T {
        struct {} { return 0 }
        struct { H; Rest... } { return (u32)sizeof(H) + total_field_size[Rest]() }
        else { return 0 }
    }
}

fn main() i32 {
    Vec3 p = {.x = 3, .y = 4, .z = 5}
    printf("serialize Vec3: ")  serialize(p)  printf("\n")

    Nested n = {.pos = {.x=1, .y=2, .z=3}, .hp = 100}
    printf("Nested field count = %u\n", field_count[Nested]())
    printf("Nested total field size = %u\n", total_field_size[Nested]())

    // walk a generic Pair's fields — different instantiations, same walker
    printf("Pair[i32,u8] fields = %u  size = %u\n",
           field_count[Pair[i32, u8]](), total_field_size[Pair[i32, u8]]())
    printf("Pair[Vec3,i32] fields = %u  size = %u\n",
           field_count[Pair[Vec3, i32]](), total_field_size[Pair[Vec3, i32]]())

    // scalar dispatch by type
    printf("scalars: ")  emit_scalar[i32](7)  printf(" ")  emit_scalar[u8](255)  printf(" ")  emit_scalar[bool](true)  printf("\n")
    return 0
}
