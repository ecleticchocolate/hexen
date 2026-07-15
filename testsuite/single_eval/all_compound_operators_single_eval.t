//@ expect val 113710
// All ten compound-assignment operators (+= -= *= /= %= &= |= ^= <<= >>=),
// each indexing through a call so a double-eval would both call idx() twice
// AND (since idx() advances a counter) read one array slot while storing to
// the next. calls must land at exactly 10 (one call per operator), not 20.
i32 calls = 0
i32[10] arr = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100}
fn idx() i32 {
    i32 r = calls
    calls = calls + 1
    return r
}
fn main() i32 {
    arr[idx()] += 5
    arr[idx()] -= 5
    arr[idx()] *= 2
    arr[idx()] /= 2
    arr[idx()] %= 7
    arr[idx()] &= 6
    arr[idx()] |= 1
    arr[idx()] ^= 255
    arr[idx()] <<= 2
    arr[idx()] >>= 2
    i32 sum = 0
    for i32 j = 0 to 10 {
        sum = sum + arr[j]
    }
    return sum * 100 + calls
}
