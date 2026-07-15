//@ expect val 160
i32 counter = 0
fn next_idx() i32 {
    counter = counter + 1
    return counter - 1
}
fn main() i32 {
    i32[5] arr = {10, 20, 30, 40, 50}
    arr[next_idx()] += 100
    return arr[0] + arr[1] + arr[2]
}
