//@ expect stdout
//@ | flat:   {"x": 3, "y": 4}
//@ | nested: {"a": {"x": 0, "y": 1}, "b": {"x": 2, "y": 3}}
//@ | custom: {"item": "widget", "price": "$12.50", "ship_to": {"x": 7, "y": 8}}
// A fully generic JSON serializer: no trait, no derive, no registration.
//  - a plain struct is recursed into STRUCTURALLY (field names via nameof(Orig,N),
//    field values via offsetof + the peeled field type H)
//  - a type with its own to_json() opts into custom output purely by HAVING the
//    method -- see Money rendering as "$12.50" rather than {"cents": 1250}
// Add a new struct and it serializes with zero changes to this code.
extern fn printf(u8* fmt, ...) i32
struct Point { i32 x  i32 y }
struct Line  { Point a  Point b }
struct Money { i64 cents }
impl Money {
    fn to_json() void { printf("\"$%d.%02d\"", self.cents / 100, self.cents % 100) }
}
struct Order { u8* item  Money price  Point ship_to }

fn fields[Orig, Walk, u32 N](Orig* p) void {
    match Walk {
        struct { H; Rest... } {
            if N > 0 { printf(", ") }
            printf("\"%s\": ", nameof(Orig, N))
            H* fp = (H*)((u8*)p + offsetof(Orig, N))
            emit(fp)
            fields[Orig, Rest, N + 1](p)
        }
        struct {  } {}
    }
}
fn emit[T](T* p) void {
    match T {
        impl { fn to_json() } { p.to_json() }
        i32  { printf("%d", *p) }
        i64  { printf("%d", *p) }
        u8*  { printf("\"%s\"", *p) }
        struct { H; Rest... } {
            printf("{")
            fields[T, T, 0](p)
            printf("}")
        }
        else { printf("null") }
    }
}
fn main() i32 {
    Point p = {.x=3, .y=4}
    printf("flat:   ")  emit(&p)  printf("\n")
    Line l = {.a={.x=0,.y=1}, .b={.x=2,.y=3}}
    printf("nested: ")  emit(&l)  printf("\n")
    Order o = {.item="widget", .price={.cents=1250}, .ship_to={.x=7,.y=8}}
    printf("custom: ")  emit(&o)  printf("\n")
    return 0
}
