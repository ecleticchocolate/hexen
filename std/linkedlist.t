// LinkedList[T] - doubly-linked list.
//
// Same blueprint as Vector/Array/HashMap. Destruction is never called by hand:
//
//   delete p      one heap object
//   delete[] p    a heap array (destroys every element, then frees)
//   scope exit    RAII, for any local
//
// Element ownership is the one shared predicate -- does T have __delete()? --
// asked in the same two places every other container asks it:
//
//   store_elem(node, item)   T comes IN.  Owning T -> item.copy(), a deep copy.
//                            Plain T -> a field-copy.
//   move_out(node)           T goes OUT. Owning T -> the node's slot is blanked
//                            after, so it stops aliasing what left.
//
// What differs from Vector is only where elements live: one heap Node per
// element rather than one contiguous buffer. So `delete node` (single object)
// replaces Vector's `delete[] data` -- and because `delete` does not recurse
// into fields on its own, Node carries a destructor that releases its element
// (see below). That is the node-per-element equivalent of what delete[] does
// for a contiguous buffer.

// Node carries a destructor so `delete n` releases the element it holds.
// `delete` calls the pointee's __delete() and then frees the storage, but it
// does NOT recurse into fields on its own -- without this, freeing a node of a
// LinkedList[Vector[u8]] released the node and leaked every inner buffer.
//
// The `match T` is the same one predicate every container uses: a plain T has
// nothing to release, so this compiles to nothing for LinkedList[i32].
struct Node[T] {
    T data
    Node[T]* next
    Node[T]* prev
}

impl Node[T] {
    fn __delete() void {
        match T {
            Owning { self.data.__delete() }
            else   { }
        }
    }
}

// Field defaults ARE the empty state: storage is not zeroed, so a bare
// `LinkedList[T] l` would otherwise hold garbage pointers that teardown would
// try to free.
pub struct LinkedList[T] {
    Node[T]* head = null
    Node[T]* tail = null
    u32 size = 0
}

pub impl LinkedList[T] {
    // Per-list step function baked into IteratorCursor's const-generic type.
    // Declared before begin(), same ordering rule as Vector/Array/HashMap.
    static fn step(struct{Node[T]* cur} * s) Option[T*] {
        if s.cur == null { return .None }
        T* p = &s.cur.data
        s.cur = s.cur.next
        return .Some(p)
    }

    static fn create() LinkedList[T] {
        LinkedList[T] l
        return l
    }

    // Single source of truth for "how does a T get stored into a node":
    // deep-copy if T owns a resource, plain value otherwise.
    fn store_elem(Node[T]* n, T item) void {
        match T {
            Owning { n.data = item.copy() }
            else   { n.data = item }
        }
    }

    // store_elem's counterpart: takes a T OUT of a node and blanks the slot
    // behind it, so an owning T stops being aliased by a node that is about to
    // be freed. Every removal routes through this.
    fn move_out(Node[T]* n) T {
        T out = n.data
        T blank
        n.data = blank
        return out
    }

    fn push_back(T item) void {
        Node[T]* n = new Node[T]
        self.store_elem(n, item)
        n.next = null
        n.prev = self.tail
        if self.tail != null { self.tail.next = n }
        else                 { self.head = n }
        self.tail = n
        self.size = self.size + 1
    }

    fn push_front(T item) void {
        Node[T]* n = new Node[T]
        self.store_elem(n, item)
        n.prev = null
        n.next = self.head
        if self.head != null { self.head.prev = n }
        else                 { self.tail = n }
        self.head = n
        self.size = self.size + 1
    }

    // Unlinks a node and frees it, returning the element it held. The element
    // is moved out FIRST, so `delete n` cannot destroy a T the caller now owns.
    fn unlink(Node[T]* n) T {
        T out = self.move_out(n)
        if n.prev != null { n.prev.next = n.next } else { self.head = n.next }
        if n.next != null { n.next.prev = n.prev } else { self.tail = n.prev }
        self.size = self.size - 1
        delete n
        return out
    }

    fn pop_back() Option[T] {
        if self.tail == null { return .None }
        return {.Some = self.unlink(self.tail)}
    }

    fn pop_front() Option[T] {
        if self.head == null { return .None }
        return {.Some = self.unlink(self.head)}
    }

    // The node at `index`, or null. Walks from whichever end is closer.
    fn node_at(u32 index) Node[T]* {
        if index >= self.size { return null }
        if index * 2 <= self.size {
            Node[T]* c = self.head
            for u32 i = 0 to index { c = c.next }
            return c
        }
        Node[T]* c = self.tail
        u32 back = self.size - 1 - index
        for u32 i = 0 to back { c = c.prev }
        return c
    }

    // Direct, unchecked access. Returns a pointer into the node, so
    // `l[i] = x` also works. See get() for the checked equivalent.
    fn __index(u32 index) T* {
        return &self.node_at(index).data
    }

    fn get(u32 index) Option[T] {
        Node[T]* n = self.node_at(index)
        if n == null { return .None }
        return {.Some = n.data}
    }

    // The displaced element is moved out first -- an ordinary local, destroyed
    // by scope exit like any other -- before store_elem overwrites the slot.
    fn set(u32 index, T item) bool {
        Node[T]* n = self.node_at(index)
        if n == null { return false }
        T old = self.move_out(n)
        self.store_elem(n, item)
        return true
    }

    fn front() Option[T] {
        if self.head == null { return .None }
        return {.Some = self.head.data}
    }

    fn back() Option[T] {
        if self.tail == null { return .None }
        return {.Some = self.tail.data}
    }

    fn insert_at(u32 index, T item) bool {
        if index > self.size { return false }
        if index == 0          { self.push_front(item)  return true }
        if index == self.size  { self.push_back(item)   return true }
        Node[T]* at = self.node_at(index)
        Node[T]* n = new Node[T]
        self.store_elem(n, item)
        n.prev = at.prev
        n.next = at
        at.prev.next = n
        at.prev = n
        self.size = self.size + 1
        return true
    }

    fn remove_at(u32 index) Option[T] {
        Node[T]* n = self.node_at(index)
        if n == null { return .None }
        return {.Some = self.unlink(n)}
    }

    fn reverse() void {
        Node[T]* c = self.head
        while c != null {
            Node[T]* nxt = c.next
            c.next = c.prev
            c.prev = nxt
            c = nxt
        }
        Node[T]* h = self.head
        self.head = self.tail
        self.tail = h
    }

    // Element-wise equality: same length, and every element equal. Elements are
    // compared with `*a == *b` rather than by value, so an element type whose
    // own __eq takes a pointer is reachable from generic code.
    fn __eq(LinkedList[T] other) bool {
        if self.size != other.size { return false }
        Node[T]* a = self.head
        Node[T]* b = other.head
        while a != null {
            T* x = &a.data
            T* y = &b.data
            if !(*x == *y) { return false }
            a = a.next
            b = b.next
        }
        return true
    }

    // `!=` dispatches to __neq on its own -- it does NOT fall back to
    // !(a == b) -- so a type defining only __eq has no `!=` at all.
    fn __neq(LinkedList[T] other) bool {
        if self.size != other.size { return true }
        Node[T]* a = self.head
        Node[T]* b = other.head
        while a != null {
            T* x = &a.data
            T* y = &b.data
            if !(*x == *y) { return true }
            a = a.next
            b = b.next
        }
        return false
    }

    // Replaces the list's contents from a flat array literal:
    //
    //     LinkedList[i32] l = {1, 2, 3}
    //
    // Same spelling Vector and Array accept. clear() releases the old nodes
    // first (each node's own destructor releasing its element), then push_back
    // routes every new element through store_elem, so an owning T deep-copies
    // rather than aliasing the literal's storage.
    fn __assign[u32 N](T[N] arr) void {
        self.clear()
        for u32 i = 0 to N {
            self.push_back(arr[i])
        }
    }

    // Deep copy. Routes through push_back, which routes through store_elem, so
    // an owning T gets its own resource per element instead of aliasing.
    fn copy() LinkedList[T] {
        LinkedList[T] out = LinkedList[T].create()
        Node[T]* c = self.head
        while c != null {
            out.push_back(c.data)
            c = c.next
        }
        return out
    }

    // Removes every element. Each node's T is destroyed by `delete n` -- there
    // is no element loop to write and no `match T` deciding whether one is
    // needed, the same way Vector's delete[] destroys its buffer's elements.
    fn clear() void {
        Node[T]* c = self.head
        while c != null {
            Node[T]* nxt = c.next
            delete c
            c = nxt
        }
        self.head = null
        self.tail = null
        self.size = 0
    }

    fn __delete() void {
        self.clear()
    }

    fn is_empty() bool {
        return self.size == 0
    }

    fn len() u32 {
        return self.size
    }

    // Iterator support for `for TYPE val in list` / `for unpack ...`. The
    // cursor's state is just the current node; the step function is baked into
    // its type, so there is no list-specific cursor implementation here.
    fn begin() IteratorCursor[T, struct{Node[T]* cur}, LinkedList[T].step] {
        return {
            .state = {
                .cur = self.head
            }
        }
    }
}
