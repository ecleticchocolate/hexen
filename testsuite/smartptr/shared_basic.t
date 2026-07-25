//@ expect stdout
//@ | p1.id=42 p1.value=100
//@ | refcount after clone: 2
//@ | p2.value after mutate: 999
//@ | refcount after p2 dies: 1
//@ | p1.value sees mutation: 999
extern fn printf(u8* fmt, ...) i32

struct Resource {
    i32 id
    i32 value
}

struct Shared[T] {
    T* ptr
    u32* refcount
}

impl Shared[T] {
    fn __assign[P](P* ptr) void {
        match P {
            T {
                self.ptr = ptr
                self.refcount = new[1] u32
                *self.refcount = 1
            }
            Shared[T] {
                self.ptr = ptr.ptr
                self.refcount = ptr.refcount
                if self.refcount != null {
                    *self.refcount = *self.refcount + 1
                }
            }
        }
    }

    fn __deref() T* {
        return self.ptr
    }

    fn __delete() void {
        if self.refcount != null {
            *self.refcount = *self.refcount - 1
            if *self.refcount == 0 {
                delete self.ptr
                delete self.refcount
            }
        }
    }
}

fn main() i32 {
    // T^ desugars to Shared[T]
    Resource^ p1 = new Resource{.id = 42, .value = 100}
    printf("p1.id=%d p1.value=%d\n", p1.id, p1.value)

    {
        Resource^ p2
        p2 = &p1
        printf("refcount after clone: %d\n", *p1.refcount)
        p2.value = 999
        printf("p2.value after mutate: %d\n", p2.value)
    }
    printf("refcount after p2 dies: %d\n", *p1.refcount)
    printf("p1.value sees mutation: %d\n", p1.value)

    return 0
}
