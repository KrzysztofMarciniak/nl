(define msg "Hello, World")
(define suffix "\n")
(define full (concat msg suffix))
(ffi "libc" "write" 1 full (ffi "libc" "strlen" full))
