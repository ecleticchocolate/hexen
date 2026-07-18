//@ expect stdout
//@ | Circle{5}
//@ | Rect{Point{x=7, y=9}}
//@ | Flag{true}
//@ | None
// A fully generic debug-dumper: dump[T](T*) prints ANY struct or enum with
// zero per-type registration, same idiom as generic_json.t (struct field-walk
// via nameof/offsetof + Rest... pack-tail), extended to real enum support --
// which needed several compiler fixes to be possible at all this session:
// enum Rest... peeling with absolute tag preservation, and NULL/void-payload
// handling inside a generic match. A struct payload (Point), a primitive
// payload (i32), a bool payload, and a genuine no-payload variant (None) all
// print correctly with no special-casing in this file for Shape specifically.
extern fn printf(u8* fmt, ...) i32

struct Point { i32 x  i32 y }
enum Shape { i32 Circle  Point Rect  bool Flag  None }

fn dump_fields[Orig, Walk, u32 N](Orig* p) void {
    match Walk {
        struct { H h  Rest... r } {
            if N > 0 { printf(", ") }
            printf("%s=", nameof(Orig, N))
            H* fp = (H*)((u8*)p + offsetof(Orig, N))
            dump(fp)
            dump_fields[Orig, Rest, N + 1](p)
        }
        struct {} {}
    }
}

// Walk an enum's variant list one absolute tag at a time. `p` always points at
// the ORIGINAL value's bytes -- Walk/H/Rest only track the CURRENT variant's
// payload type/name (via Orig+N) and when to stop.
fn dump_variant[Orig, Walk, u32 N](Orig* p, u32 tag) void {
    match Walk {
        enum { H h  Rest... r } {
            if tag == N {
                printf("%s", nameof(Orig, N))
                match H {
                    void { }
                    else {
                        printf("{")
                        H* payload = (H*)((u8*)p + 4)
                        dump(payload)
                        printf("}")
                    }
                }
            } else {
                dump_variant[Orig, Rest, N + 1](p, tag)
            }
        }
        enum {} {}
    }
}

fn dump[T](T* p) void {
    match T {
        i32  { printf("%d", *p) }
        u32  { printf("%d", *p) }
        bool { if (*p) { printf("true") } else { printf("false") } }
        f32  { printf("%f", *p) }
        u8*  { printf("\"%s\"", *p) }
        impl { fn dump_self() } { p.dump_self() }
        enum { H h  Rest... r } {
            u32 tag = *(u32*)p
            dump_variant[T, T, 0](p, tag)
        }
        struct { H h  Rest... r } {
            printf("%s{", nameof(T))
            dump_fields[T, T, 0](p)
            printf("}")
        }
        else { printf("<?>") }
    }
}

fn main() i32 {
    Shape s1 = .Circle(5)
    Shape s2 = .Rect({7, 9})
    Shape s3 = .Flag(true)
    Shape s4 = .None
    dump(&s1)  printf("\n")
    dump(&s2)  printf("\n")
    dump(&s3)  printf("\n")
    dump(&s4)  printf("\n")
    return 0
}
