//@ expect val 14
fn main() i32 {
    u32[2][5] g
    u32 i = 0
    while i < 2 {
        u32 j = 0
        while j < 5 { g[i][j] = i * 10 + j  j = j + 1 }
        i = i + 1
    }
    return (i32)(g[0][4] + g[1][0])
}
