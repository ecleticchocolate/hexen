//@ expect stdout
//@ | === Zero-Copy *rest... Pack Traversal ===
//@ | Visiting Pig: The pig says: wee wee
//@ | Visiting Dog: The dog says: bow wow
//@ | Visiting Cat: The cat says: meow
extern fn printf(u8* fmt, ...) i32

struct Pig {}  impl Pig { fn sound() { printf("The pig says: wee wee\n") } }
struct Dog {}  impl Dog { fn sound() { printf("The dog says: bow wow\n") } }
struct Cat {}  impl Cat { fn sound() { printf("The cat says: meow\n") } }

fn emit_sound[T](T* obj) void {
    match T {
        impl { fn sound() } { obj.sound() }
        else { printf("silent\n") }
    }
}

fn visit_pack[T](T* args) void {
    match T {
        struct { H; Rest... } {
            unpack {*head, *rest...} = *args
            printf("Visiting %s: ", nameof(H))
            emit_sound(head)
            visit_pack(rest)
        }
        struct {} { }
    }
}

struct Zoo[Ts...] {
    Ts animals
}

fn main() i32 {
    printf("=== Zero-Copy *rest... Pack Traversal ===\n")
    Zoo[Pig, Dog, Cat] z = { .animals = { {}, {}, {} } }
    visit_pack(&z.animals)
    return 0
}
