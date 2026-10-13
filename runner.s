    .section .rodata
input_state:
    .asciz "21345671111111"

.if RENDER
    .balign 4
led_palette:
    .word 0x00FFFFFF     # 0: U, white
    .word 0x00FFA500     # 1: L, orange
    .word 0x0000FF00     # 2: F, green
    .word 0x00FF0000     # 3: R, red
    .word 0x000000FF     # 4: B, blue
    .word 0x00FFFF00     # 5: D, yellow

    .balign 2
led_sticker_offsets:
    # U
    .half 4*( 0*LED_MATRIX_0_WIDTH+ 9), 4*( 0*LED_MATRIX_0_WIDTH+13)
    .half 4*( 3*LED_MATRIX_0_WIDTH+ 9), 4*( 3*LED_MATRIX_0_WIDTH+13)
    # L
    .half 4*( 7*LED_MATRIX_0_WIDTH+ 0), 4*( 7*LED_MATRIX_0_WIDTH+ 4)
    .half 4*(10*LED_MATRIX_0_WIDTH+ 0), 4*(10*LED_MATRIX_0_WIDTH+ 4)
    # F
    .half 4*( 7*LED_MATRIX_0_WIDTH+ 9), 4*( 7*LED_MATRIX_0_WIDTH+13)
    .half 4*(10*LED_MATRIX_0_WIDTH+ 9), 4*(10*LED_MATRIX_0_WIDTH+13)
    # R
    .half 4*( 7*LED_MATRIX_0_WIDTH+18), 4*( 7*LED_MATRIX_0_WIDTH+22)
    .half 4*(10*LED_MATRIX_0_WIDTH+18), 4*(10*LED_MATRIX_0_WIDTH+22)
    # B
    .half 4*( 7*LED_MATRIX_0_WIDTH+27), 4*( 7*LED_MATRIX_0_WIDTH+31)
    .half 4*(10*LED_MATRIX_0_WIDTH+27), 4*(10*LED_MATRIX_0_WIDTH+31)
    # D
    .half 4*(14*LED_MATRIX_0_WIDTH+ 9), 4*(14*LED_MATRIX_0_WIDTH+13)
    .half 4*(17*LED_MATRIX_0_WIDTH+ 9), 4*(17*LED_MATRIX_0_WIDTH+13)

led_move_cycles:
    .byte 1,18,21,9,  3,16,23,11, 12,13,15,14
    .byte 0,6,23,13,  1,4,22,15,  16,17,19,18
    .byte 6,10,14,18, 7,11,15,19, 20,21,23,22

led_inverse_moves:
    .byte 2,1,0, 5,4,3, 8,7,6

led_move_names:
    .asciz "R \0", "R2 ", "R' "
    .asciz "B \0", "B2 ", "B' "
    .asciz "D \0", "D2 ", "D' "
.endif

.section .bss
    .balign 16
result_buffer:
    .space 12              # solution_t: length + 11 moves

    .if RENDER
led_stickers:
    .space 24
.endif

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

    mv s0, a0

.if RENDER
    bltz s0, led_render_skip
    la a0, result_buffer
    call led_play_solution
led_render_skip:
    mv a0, s0
.endif

    # Print solution length (negative means failure)
    li a7, 1
    ecall

    # Print newline
    li a0, 10
    li a7, 11
    ecall

    # Exit
    slt a0, s0, zero
    li a7, 93
    ecall




.if RENDER
.equ LED_ROW_BYTES, 4*LED_MATRIX_0_WIDTH


# Initialize solved cube
# led_stickers[i] = i >> 2
led_init_solved:
    la t0, led_stickers
    li t1, 0
    li t2, 24

led_init_solved_loop:
    srli t3, t1, 2
    sb t3, 0(t0)
    addi t0, t0, 1
    addi t1, t1, 1
    blt t1, t2, led_init_solved_loop
    ret


# Draw one sticker
# a0 = sticker index (0..23)
led_draw_sticker:
    la t0, led_stickers
    add t0, t0, a0
    lbu t1, 0(t0)

    # Get RGB from palette
    slli t1, t1, 2
    la t2, led_palette
    add t2, t2, t1
    lw t1, 0(t2)

    # Get LED address
    la t2, led_sticker_offsets
    slli t3, a0, 1
    add t2, t2, t3
    lhu t3, 0(t2)

    li t0, LED_MATRIX_0_BASE
    add t0, t0, t3

    # Draw 4x3 rectangle
    sw t1, 0(t0)
    sw t1, 4(t0)
    sw t1, 8(t0)
    sw t1, 12(t0)

    sw t1, LED_ROW_BYTES(t0)
    sw t1, LED_ROW_BYTES+4(t0)
    sw t1, LED_ROW_BYTES+8(t0)
    sw t1, LED_ROW_BYTES+12(t0)

    sw t1, 2*LED_ROW_BYTES(t0)
    sw t1, 2*LED_ROW_BYTES+4(t0)
    sw t1, 2*LED_ROW_BYTES+8(t0)
    sw t1, 2*LED_ROW_BYTES+12(t0)
    ret


# Clear entire LED matrix
led_clear:
    li t0, LED_MATRIX_0_BASE
    li t1, LED_MATRIX_0_SIZE
    add t1, t1, t0

led_clear_loop:
    bgeu t0, t1, led_clear_done
    sw zero, 0(t0)
    addi t0, t0, 4
    j led_clear_loop

led_clear_done:
    ret


# Draw all 24 stickers
led_draw_all:
    addi sp, sp, -16
    sw ra, 12(sp)
    sw s0, 8(sp)

    li s0, 0

led_draw_all_loop:
    li t0, 24
    bge s0, t0, led_draw_all_done

    mv a0, s0
    call led_draw_sticker

    addi s0, s0, 1
    j led_draw_all_loop

led_draw_all_done:
    lw s0, 8(sp)
    lw ra, 12(sp)
    addi sp, sp, 16
    ret


# Apply move to sticker array
# a0 = move ID (0..8)
# return a0 = affected sticker indices
led_apply_move:
    li t0, 6
    bgeu a0, t0, led_apply_D
    li t0, 3
    bgeu a0, t0, led_apply_B

    mv t1, a0
    la t2, led_move_cycles
    j led_apply_ready

led_apply_B:
    addi t1, a0, -3
    la t2, led_move_cycles
    addi t2, t2, 12
    j led_apply_ready

led_apply_D:
    addi t1, a0, -6
    la t2, led_move_cycles
    addi t2, t2, 24

led_apply_ready:
    la t3, led_stickers
    mv t4, t2
    li t5, 3

led_apply_cycle:
    lbu a1, 0(t4)
    lbu a2, 1(t4)
    lbu a3, 2(t4)
    lbu a4, 3(t4)

    add a5, t3, a1
    add a6, t3, a2
    add a7, t3, a3
    add t6, t3, a4

    lbu a1, 0(a5)
    lbu a2, 0(a6)
    lbu a3, 0(a7)
    lbu a4, 0(t6)

    beqz t1, led_apply_quarter
    li t0, 1
    beq t1, t0, led_apply_half

    # Inverse: (B,C,D,A)
    sb a2, 0(a5)
    sb a3, 0(a6)
    sb a4, 0(a7)
    sb a1, 0(t6)
    j led_apply_next

led_apply_half:
    # Half: (C,D,A,B)
    sb a3, 0(a5)
    sb a4, 0(a6)
    sb a1, 0(a7)
    sb a2, 0(t6)
    j led_apply_next

led_apply_quarter:
    # Quarter: (D,A,B,C)
    sb a4, 0(a5)
    sb a1, 0(a6)
    sb a2, 0(a7)
    sb a3, 0(t6)

led_apply_next:
    addi t4, t4, 4
    addi t5, t5, -1
    bnez t5, led_apply_cycle

    mv a0, t2
    ret


# Delay between animation frames (add breakpoint here)
led_delay:
    ret


# a0 -> [length, move IDs...]
led_play_solution:
    addi sp, sp, -32
    sw ra, 28(sp)
    sw s0, 24(sp)
    sw s1, 20(sp)
    sw s2, 16(sp)
    sw s3, 12(sp)
    sw a0, 8(sp)

    lbu s1, 0(a0)
    addi s0, a0, 1
    add s1, s0, s1
    call led_init_solved

    # Reconstruct scramble by applying inverse moves backwards
    mv s2, s1
led_play_reverse:
    beq s2, s0, led_play_initial
    addi s2, s2, -1
    lbu a0, 0(s2)
    la t0, led_inverse_moves
    add t0, t0, a0
    lbu a0, 0(t0)
    call led_apply_move
    j led_play_reverse

led_play_initial:
    call led_clear
    call led_draw_all
    call led_delay

    # Play solution forwards
led_play_forward:
    beq s0, s1, led_play_print_init
    lbu a0, 0(s0)
    addi s0, s0, 1
    call led_apply_move
    mv s2, a0
    li s3, 12

led_play_draw:
    lbu a0, 0(s2)
    call led_draw_sticker
    addi s2, s2, 1
    addi s3, s3, -1
    bnez s3, led_play_draw
    call led_delay
    j led_play_forward

    # Print solution after animation
led_play_print_init:
    lw s0, 8(sp)
    addi s0, s0, 1

led_play_print:
    beq s0, s1, led_play_done
    lbu a0, 0(s0)
    slli a0, a0, 2
    la t0, led_move_names
    add a0, a0, t0
    li a7, 4
    ecall
    addi s0, s0, 1
    j led_play_print

led_play_done:
    li a0, 10
    li a7, 11
    ecall

    lw s3, 12(sp)
    lw s2, 16(sp)
    lw s1, 20(sp)
    lw s0, 24(sp)
    lw ra, 28(sp)
    addi sp, sp, 32
    ret

.endif
