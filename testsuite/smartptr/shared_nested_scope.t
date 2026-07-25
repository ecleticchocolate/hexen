//@ expect stdout
//@ | rc=1
//@ | rc=2
//@ | rc=3
//@ | rc=2
//@ | rc=1
//@ | freed id=7
extern fn printf(u8* fmt, ...) i32

struct Obj { i32 id }

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
                printf("freed id=%d\n", self.ptr.id)
                delete self.ptr
                delete self.refcount
            }
        }
    }
}

fn main() i32 {
    Obj^ a = new Obj{.id = 7}
    printf("rc=%d\n", *a.refcount)
    {
        Obj^ b
        b = &a
        printf("rc=%d\n", *a.refcount)
        {
            Obj^ c
            c = &b
            printf("rc=%d\n", *a.refcount)
        }
        printf("rc=%d\n", *a.refcount)
    }
    printf("rc=%d\n", *a.refcount)
    return 0
}
