//@ expect stdout
//@ | alive id=1
//@ | alive id=2
//@ | destroying id=2
//@ | destroying id=1
extern fn printf(u8* fmt, ...) i32

struct Obj { i32 id }

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
            printf("destroying id=%d\n", self.ptr.id)
            delete self.ptr
        }
    }
}

fn main() i32 {
    Obj' a = new Obj{.id = 1}
    printf("alive id=%d\n", a.id)
    {
        Obj' b = new Obj{.id = 2}
        printf("alive id=%d\n", b.id)
        // b destroyed here
    }
    // a destroyed here
    return 0
}
