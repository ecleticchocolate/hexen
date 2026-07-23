//@ expect err function default argument must be a constant expression (constexpr)

extern fn get_runtime_port() i32

// get_runtime_port() is an extern runtime function (non-constexpr) -> REJECTED!
fn connect(i32 port = get_runtime_port()) i32 {
    return port
}

fn main() i32 {
    return connect()
}
