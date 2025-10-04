; Olang I/O Library - Pure LLVM IR implementation
; Provides basic input/output functions

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Declare C standard library functions
declare i32 @printf(ptr, ...)
declare i32 @puts(ptr)
declare i32 @putchar(i32)

; String constants
@.str_int = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@.str_hello = private unnamed_addr constant [13 x i8] c"Hello World\0A\00", align 1

; Print integer
define void @print_int(i32 %num) {
entry:
  %call = call i32 (ptr, ...) @printf(ptr @.str_int, i32 %num)
  ret void
}

; Print string
define void @print_str(ptr %str) {
  entry:
    %call = call i32 @puts(ptr %str)
    ret void
}

; Print newline
define void @print_newline() {
  entry:
    %call = call i32 @putchar(i32 10)
    ret void
}

; Print Hello World
define void @print_hello() {
  entry:
    %call = call i32 @puts(ptr @.str_hello)
    ret void
}

; Version using Linux system calls (write syscall)
define i32 @sys_write(i32 %fd, ptr %buf, i64 %count) {
entry:
  ; syscall number for write is 1
  %syscall_num = add i32 0, 1
  %result = call i64 asm sideeffect "syscall", 
    "={rax},{rax},{rdi},{rsi},{rdx},~{rcx},~{r11}" 
    (i64 1, i32 %fd, ptr %buf, i64 %count)
  %ret = trunc i64 %result to i32
  ret i32 %ret
}

; Simple print integer (using syscall)
define void @sys_print_int(i32 %num) {
entry:
  ; Pre-allocate buffer for integer to string conversion
  %buffer = alloca [20 x i8], align 1
  
  ; Simplified: only handle small positive integers
  ; Should implement full itoa in practice
  ; Using printf directly is more reliable here
  %call = call i32 (ptr, ...) @printf(ptr @.str_int, i32 %num)
  ret void
}

