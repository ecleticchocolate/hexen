//@ expect stdout
//@ | p.id=1 p.value=42
//@ | after mutate: p.value=100
//@ | destroying resource id=1
extern fn printf(u8* fmt, ...) i32

struct Resource {
    i32 id
    i32 value
}

struct Unique[T] {
    T* ptr
}

impl Unique[T] {
    fn __assign[P](P* ptr) void {
        match P {
            T {
                self.ptr = ptr
            }
        }
    }

    fn __deref() T* {
        return self.ptr
    }

    fn __delete() void {
        if self.ptr != null {
            printf("destroying resource id=%d\n", self.ptr.id)
            delete self.ptr
        }
    }
}

fn main() i32 {
    // T' desugars to Unique[T]
    Resource' p = new Resource{.id = 1, .value = 42}
    printf("p.id=%d p.value=%d\n", p.id, p.value)

    // mutate through auto-deref
    p.value = 100
    printf("after mutate: p.value=%d\n", p.value)

    // p goes out of scope here -> __delete fires
    return 0
}
