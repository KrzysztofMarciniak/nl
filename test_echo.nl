(define write
  (lambda (str)
    (ffi "libc" "write" 1 str (ffi "libc" "strlen" str))
    nil))

(define double
  (lambda (x)
    (* x 2)))

(define number->string
  (lambda (n)
    (concat "" n)))

(write "Hello, World!\n")

(write (number->string (double 2)))
