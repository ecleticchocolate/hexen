// Shared cursor machinery for the begin()/next() iterator protocol.
// Containers provide only a state type and a step function; they do not need
// to declare a cursor struct of their own.

pub struct IteratorCursor[E, S, fn(S*) Option[E*] F] {
    S state
}

pub impl IteratorCursor[E, S, F] {
    // The step function yields a pointer so callers can choose ordinary
    // by-value iteration (`for E x`) or write-through iteration (`for E* x`).
    fn next() Option[E*] {
        return F(&self.state)
    }
}
