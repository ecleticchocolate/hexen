//@ expect val 150

fn sum_impl[T](T args) i32 {
    i32 result = 0
    match T {
        struct { i32; Rest... } {
            result = args._0 + sum_impl((Rest) args)
        }
        struct {  } {
            result = 0
        }
    }
    return result
}

fn sum[T](T... args) i32 {
    return sum_impl(args)
}

fn main() i32 {
    return sum(10, 20, 30, 40, 50)
}
