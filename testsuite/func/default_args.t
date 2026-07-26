//@ expect val 1000
fn connect(i32 port = 8080, i32 timeout = 30) i32 {
    return port + timeout
}

struct Server {
    i32 base_port
}

impl Server {
    fn listen(i32 port = 9000, i32 clients = 100) i32 {
        return self.base_port + port + clients
    }
}

fn add_default[T](T val, T bonus = 5) T {
    return val + bonus
}

fn main() i32 {
    // 1. connect() -> 8080 + 30 = 8110
    i32 c1 = connect()
    // 2. connect(500) -> 500 + 30 = 530
    i32 c2 = connect(500)
    // 3. connect(100, 20) -> 100 + 20 = 120
    i32 c3 = connect(100, 20)

    // 4. Server.listen() -> 10 + 9000 + 100 = 9110
    Server s = {.base_port = 10}
    i32 s1 = s.listen()
    // 5. Server.listen(7000) -> 10 + 7000 + 100 = 7110
    i32 s2 = s.listen(7000)

    // 6. Generic default argument: add_default[i32](40) -> 40 + 5 = 45
    i32 g1 = add_default[i32](40)

    // (8110 + 530 + 120) - 9110 + 7110 + 45 - 5805 = 1000
    i32 total = (c1 + c2 + c3) - s1 + s2 + g1 - 5805
    return total
}
