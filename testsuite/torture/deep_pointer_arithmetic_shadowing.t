//@ expect val 190
fn extract[T](T* ptr) T { return *ptr }
fn main() i32 {
    i32[5] arr = {10, 20, 30, 40, 50}
    i32* p = &arr[0]
    i32 sum = 0
    {
        i32* p = &arr[1]
        sum = sum + extract(p)
        {
            i32* p = &arr[2]
            sum = sum + extract(p)
            {
                i32* p = &arr[3]
                sum = sum + extract(p)
                {
                    i32* p = &arr[4]
                    sum = sum + extract(p)
                }
                sum = sum + extract(p)
            }
        }
    }
    sum = sum + extract(p)
    return sum
}
