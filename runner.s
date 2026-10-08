    .section .rodata
input_state:
    .asciz "21345671111111"

    .section .bss
    .balign 16
result_buffer:
    .space 12              # solution_t: length + 11 moves

    .balign 16
stack_bottom:
    .space 8192
stack_top:

    .section .text
    .globl _start
_start:
    # Initialize stack
    la sp, stack_top

    # solve_cube(input_state, &result_buffer)
    la a0, input_state
    la a1, result_buffer
    call solve_cube

    # Print solution length (negative means failure)
    mv s0, a0
    li a7, 1
    ecall

    # Print newline
    li a0, 10
    li a7, 11
    ecall

    # Exit status: 0 on success, 1 on failure
    slt a0, s0, zero
    li a7, 93
    ecall
