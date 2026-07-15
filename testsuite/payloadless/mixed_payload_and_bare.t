//@ expect val 43
enum Mixed { u32 WithPayload  Empty }
fn main() i32 {
    Mixed a = .WithPayload{42}
    Mixed b = .Empty
    i32 r = 0
    match a {
        .WithPayload{v} { r = r + (i32)v }
        .Empty { r = r + 1000 }
    }
    match b {
        .WithPayload{v} { r = r + (i32)v }
        .Empty { r = r + 1 }
    }
    return r
}
