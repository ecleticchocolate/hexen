//@ expect val 99
i32 g_val = 0

fn test_basic() {
    g_val += 1
    defer g_val += 10
    g_val += 100
}

fn test_return() i32 {
    g_val += 1
    defer g_val += 10
    g_val += 100
    return 42
}

fn test_loop() {
    for u32 i = 0 to 2 {
        defer g_val += 10
        if i == 0 {
            defer g_val += 100
            continue
        }
        defer g_val += 1000
        break
    }
}

fn main() i32 {
    g_val = 0
    test_basic()
    if g_val != 111 { return 1 }
    
    g_val = 0
    i32 r = test_return()
    if r != 42 { return 2 }
    if g_val != 111 { return 3 }
    
    g_val = 0
    test_loop()
    if g_val != 1120 { return 4 }
    
    return 99
}
