(define (echo s)
  (ffi "libc" "my_strlen" s)
  (ffi "libc" "write" 1 s (ffi "libc" "my_strlen" s)))
