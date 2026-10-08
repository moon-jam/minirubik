.equ CUBIES, 7
.equ PERMUTATIONS, 5040
.equ ORIENTATIONS, 729
.equ STATES, PERMUTATIONS * ORIENTATIONS		# Pre-multiply by assembler
.equ MOVES, 9
.equ HALF_CLASSES, 210
.equ HALF_ENTRIES, HALF_CLASSES * ORIENTATIONS
.equ MAX_DEPTH, 11

.equ COORD_P, 0
.equ COORD_O, 2

.equ SOLUTION_LENGTH, 0
.equ SOLUTION_MOVES, 1

# Precomputed transition and distance tables (generated separately).
.include "build/solver-data.inc"
.text

# permutation_bound(p): admissible lower bound from permutation only.
# Input: a0=p. Output: a0=lower bound (0..15).
# Each byte packs two 4-bit distances.
.globl permutation_bound
permutation_bound:
# a0 = permutation index
    srli t0, a0, 1
    andi t1, a0, 1
    slli t1, t1, 2
    la	 t2, permutation_distance
    add	 t2, t0, t2
    lbu	 a0, 0(t2)
    srl	 a0, a0, t1
    andi a0, a0,	15
	ret


# half_remainder(p, o): subgroup distance modulo 3.
# Input: a0=p, a1=o. Output: a0=remainder (0..2).
# Four 2-bit entries are packed into each byte.
.globl half_remainder
half_remainder:
# index = subgroup_offset[p] + o
	slli t0, a0, 2
	la   t1, subgroup_offset
	add  t1, t1, t0
	lw   t1, 0(t1)
	add  t1, t1, a1

# Extract 2-bit remainder
	andi t2, t1, 3
	slli t2, t2, 1
	srli t1, t1, 2
	la   t0, subgroup_remainder
	add  t0, t0, t1
	lbu  a0, 0(t0)
	srl  a0, a0, t2
	andi a0, a0, 3
	ret


# quarter_rank(p, o, face): apply one clockwise quarter-turn.
# Inputs: a0=p, a1=o, a2=face (R=0, B=1, D=2).
# Outputs: a0=new p, a1=new o. Table entries are unsigned 16-bit.
.globl quarter_rank
quarter_rank:
# New permutation: permutation_rows[face][p]
	slli t0, a2, 2
	la   t1, permutation_rows
	add  t1, t1, t0
	lw   t1, 0(t1)
	slli t2, a0, 1
	add  t1, t1, t2
	lhu  a0, 0(t1)

# New orientation: orientation_rows[face][o]
	la   t1, orientation_rows
	add  t1, t1, t0
	lw   t1, 0(t1)
	slli t2, a1, 1
	add  t1, t1, t2
	lhu  a1, 0(t1)
	ret



# Scratch buffers: one parsed permutation and 12 DFS frames (16 bytes each).
# Global scratch makes the solver non-reentrant.
.section .bss
.balign 4
p_buffer: .space 7
.balign 16
frames: .space 192

.text
# parse_encode(input): validate PPPPPPPOOOOOOO and encode coordinates.
# Input: a0=pointer to a 14-character NUL-terminated string.
# Output: a0=permutation rank, a1=orientation rank; invalid => a0=-2.
# Leaf function: only caller-saved registers are used.
.globl parse_encode
parse_encode:
    # a0: input string; a0/a1: ranks; -2 if invalid
    beqz a0, parse_bad
    la t0, p_buffer
    li t1, 0                  # seen mask
    li t2, 0                  # index
parse_perm:
    # Convert characters 1..7 to unique cubie indices 0..6.
    # t1 is a seven-bit seen mask; t2 is the input position.
    add t3, a0, t2
    lbu t4, 0(t3)
    addi t4, t4, -49
    sltiu t5, t4, 7
    beqz t5, parse_bad
    li t5, 1
    sll t5, t5, t4
    and t6, t1, t5
    bnez t6, parse_bad
    or t1, t1, t5
    add t3, t0, t2
    sb t4, 0(t3)
    addi t2, t2, 1
    li t3, 7
    blt t2, t3, parse_perm

    li t1, 0                  # orientation rank
    li t2, 0                  # sum
    li t3, 0                  # index
parse_orient:
    # Convert characters 1..3 to twists 0..2.
    # The first six twists form a base-3 rank; the seventh is checked only.
    addi t4, t3, 7
    add t4, a0, t4
    lbu t5, 0(t4)
    addi t5, t5, -49
    sltiu t6, t5, 3
    beqz t6, parse_bad
    add t2, t2, t5
    li t6, 6
    bge t3, t6, parse_orient_last
    slli t4, t1, 1
    add t1, t1, t4
    add t1, t1, t5
parse_orient_last:
    addi t3, t3, 1
    li t4, 7
    blt t3, t4, parse_orient
    lbu t3, 14(a0)
    bnez t3, parse_bad
    # Valid total twists are 0, 3, 6, 9, 12 (sum divisible by 3).
    li t3, 0x1249
    srl t3, t3, t2
    andi t3, t3, 1
    beqz t3, parse_bad

    # Compute Lehmer rank in radices 6,5,4,3,2.
    mv a1, t1
    li a0, 0                 # rank
    li t2, 0                 # i
    la t0, p_buffer
parse_rank_outer:
    # Lehmer digit: number of later entries smaller than p[i].
    add t3, t0, t2
    lbu t4, 0(t3)             # p[i]
    addi t5, t2, 1           # j
    li t6, 0                 # smaller
parse_rank_inner:
    li t1, 7
    bge t5, t1, parse_rank_done_inner
    add t1, t0, t5
    lbu t1, 0(t1)
    sltu t1, t1, t4
    add t6, t6, t1
    addi t5, t5, 1
    j parse_rank_inner
parse_rank_done_inner:
    li t1, 7
    sub t1, t1, t2          # radix: 7-i (first digit uses 6 but rank=0)
    # At i=0 rank starts at zero, so multiplier 7 is harmless.
    # For i=1..5 the radices are 6,5,4,3,2.
    # Repeated addition avoids the RISC-V M (multiply) extension.
    li t3, 0
parse_rank_mul:
    beqz t1, parse_rank_mul_done
    add t3, t3, a0
    addi t1, t1, -1
    j parse_rank_mul
parse_rank_mul_done:
    add a0, t3, t6
    addi t2, t2, 1
    li t1, 6
    blt t2, t1, parse_rank_outer
    ret
parse_bad:
    li a0, -2
    ret

# root_distance(p, o, r): recover complete distance to the half-turn subgroup.
# Inputs: a0=p, a1=o, a2=distance mod 3. Output: a0=distance or -1.
# Descend by choosing a move whose remainder is one less modulo 3.
# This function calls other functions, so ra and used s-registers are saved.
.globl root_distance
root_distance:
    addi sp, sp, -48
    sw ra, 44(sp)
    sw s0, 40(sp)
    sw s1, 36(sp)
    sw s2, 32(sp)
    sw s3, 28(sp)
    sw s4, 24(sp)
    sw s5, 20(sp)
    sw s6, 16(sp)
    mv s0, a0                 # current p
    mv s1, a1                 # current o
    mv s2, a2                 # remainder
    li s3, 0                  # steps
root_loop:
    # Subgroup membership is offset[p]==0 AND o==0; r==0 alone is insufficient.
    la t0, subgroup_offset
    slli t1, s0, 2
    add t0, t0, t1
    lw t0, 0(t0)
    bnez t0, root_not_done
    beqz s1, root_success
root_not_done:
    addi s4, s2, -1          # wanted
    bgez s4, root_wanted_ok
    li s4, 2
root_wanted_ok:
    li s5, 0                 # face
root_face:
    # Restart candidates at the current state for each of the three faces.
    mv s6, s0                # candidate p
    mv t0, s1                # candidate o, store on stack across calls
    sw t0, 12(sp)
    li t0, 0
    sw t0, 8(sp)             # turn
root_turn:
    # Repeated quarter-turns enumerate R/R2/R', B/B2/B', D/D2/D'.
    # Candidate orientation/turn counter live on the stack across calls.
    mv a0, s6
    lw a1, 12(sp)
    mv a2, s5
    call quarter_rank
    mv s6, a0
    sw a1, 12(sp)
    call half_remainder      # a0=new p; a1=new o already
    beq a0, s4, root_found
    lw t0, 8(sp)
    addi t0, t0, 1
    sw t0, 8(sp)
    li t1, 3
    blt t0, t1, root_turn
    addi s5, s5, 1
    li t0, 3
    blt s5, t0, root_face
    li a0, -1
    j root_exit
root_found:
    # Accept the first strictly descending remainder and repeat.
    mv s0, s6
    lw s1, 12(sp)
    mv s2, s4
    addi s3, s3, 1
    li t0, 11
    bgt s3, t0, root_failure
    j root_loop
root_failure:
    li a0, -1
    j root_exit
root_success:
    mv a0, s3
root_exit:
    lw s6, 16(sp)
    lw s5, 20(sp)
    lw s4, 24(sp)
    lw s3, 28(sp)
    lw s2, 32(sp)
    lw s1, 36(sp)
    lw s0, 40(sp)
    lw ra, 44(sp)
    addi sp, sp, 48
    ret

# apply_rank_move(p, o, move): replay a half-turn-metric move.
# Inputs: a0=p, a1=o, a2=move (0..8). Outputs: a0=new p, a1=new o.
# move = 3*face + (times-1), times=1..3 quarter-turns.
.globl apply_rank_move
apply_rank_move:
    addi sp, sp, -32
    sw ra, 28(sp)
    sw s0, 24(sp)
    sw s1, 20(sp)
    sw s2, 16(sp)
    sw s3, 12(sp)
    mv s0, a0
    mv s1, a1
    li s2, 0
    li t0, 6
    blt a2, t0, apply_move_mid
    li s2, 2
    addi s3, a2, -5
    j apply_move_loop
apply_move_mid:
    li t0, 3
    blt a2, t0, apply_move_low
    li s2, 1
    addi s3, a2, -2
    j apply_move_loop
apply_move_low:
    addi s3, a2, 1
apply_move_loop:
    # quarter_rank may overwrite a/t registers; keep coordinates, face, count in s.
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call quarter_rank
    mv s0, a0
    mv s1, a1
    addi s3, s3, -1
    bnez s3, apply_move_loop
    mv a0, s0
    mv a1, s1
    lw s3, 12(sp)
    lw s2, 16(sp)
    lw s1, 20(sp)
    lw s0, 24(sp)
    lw ra, 28(sp)
    addi sp, sp, 32
    ret

# search(p, o, solution): iterative-deepening A* (IDA*) with explicit DFS frames.
# Inputs: a0=p, a1=o, a2=solution pointer. Returns a0=length or -1.
# Heuristic: max(complete subgroup distance, permutation-only bound).
# Each move is one of nine R/R2/R'/B/B2/B'/D/D2/D' choices.
# Frame offsets: p=0,o=2,next_p=4,next_o=6 (16-bit); distance=8,
# previous_face=9,face=10,turn=11,next_distance=12,
# next_remainder=13,remainder=14 (8-bit). Frame size=16 bytes.
.globl search
search:
    addi sp, sp, -64
    sw ra, 60(sp)
    sw s0, 56(sp)
    sw s1, 52(sp)
    sw s2, 48(sp)
    sw s3, 44(sp)
    sw s4, 40(sp)
    sw s5, 36(sp)
    sw s6, 32(sp)
    sw s7, 28(sp)
    sw s8, 24(sp)
    sw s9, 20(sp)
    sw s10, 16(sp)
    sw s11, 12(sp)
    mv s0, a0                 # root p
    mv s1, a1                 # root o
    mv s2, a2                 # solution
    beqz s0, search_p0
    j search_start
search_p0:
    bnez s1, search_start
    sb zero, 0(s2)
    li a0, 0
    j search_exit
search_start:
    mv a0, s0
    mv a1, s1
    call half_remainder
    mv s6, a0                 # root remainder
    mv a2, a0
    mv a0, s0
    mv a1, s1
    call root_distance
    bltz a0, search_failed
    mv s7, a0                 # root distance
    mv a0, s0
    call permutation_bound
    mv s3, a0                 # bound
    bge s3, s7, search_pass
    mv s3, s7
search_pass:
    # Start a new depth-first traversal with the current f=g+h limit.
    li t0, 11
    bgt s3, t0, search_failed
    li s4, 0                  # depth
    la s5, frames
    sh s0, 0(s5)
    sh s1, 2(s5)
    sb s7, 8(s5)
    li t0, 255
    sb t0, 9(s5)
    sb zero, 10(s5)
    sb zero, 11(s5)
    sb s6, 14(s5)
search_node:
    # Recover frames[depth]; depth*16 uses a shift, not multiplication.
    la s5, frames
    slli t0, s4, 4
    add s5, s5, t0
    lbu t0, 10(s5)            # face
    lbu t1, 9(s5)             # previous face
    bne t0, t1, search_face_ready
    addi t0, t0, 1
    sb t0, 10(s5)
    sb zero, 11(s5)
search_face_ready:
    # At most three faces; when exhausted, backtrack to parent.
    li t1, 3
    blt t0, t1, search_try
    beqz s4, search_next_pass
    addi s4, s4, -1
    j search_node
search_try:
    # Save the face BEFORE advancing the parent cursor.
    # Siblings cannot use the same face as the move into this frame.
    mv s8, t0                 # face for child and move
    lbu t1, 11(s5)
    bnez t1, search_continue_turn
    lhu t2, 0(s5)
    sh t2, 4(s5)
    lhu t2, 2(s5)
    sh t2, 6(s5)
    lbu t2, 8(s5)
    sb t2, 12(s5)
    lbu t2, 14(s5)
    sb t2, 13(s5)
search_continue_turn:
    # Advance the same face another quarter-turn. The stored next state
    # represents R, then R2, then R' (or the corresponding other face).
    lhu a0, 4(s5)
    lhu a1, 6(s5)
    mv a2, s8
    call quarter_rank
    sh a0, 4(s5)
    sh a1, 6(s5)
    call half_remainder
    mv s9, a0                 # new remainder
    lbu t0, 13(s5)
    # Consecutive subgroup distances differ by +/-1. Convert mod-3
    # remainder differences +/-2 to their corresponding +/-1 changes.
    sub t1, s9, t0            # delta
    li t2, -2
    bne t1, t2, search_delta_pos
    li t1, 1
search_delta_pos:
    li t2, 2
    bne t1, t2, search_delta_done
    li t1, -1
search_delta_done:
    lbu t2, 12(s5)
    add t2, t2, t1
    sb t2, 12(s5)
    sb s9, 13(s5)
    lbu t0, 11(s5)
    slli t1, s8, 1
    add t1, t1, s8
    add s10, t1, t0           # move
    addi t0, t0, 1
    li t1, 3
    blt t0, t1, search_advance
    sb zero, 11(s5)
    lbu t0, 10(s5)
    addi t0, t0, 1
    sb t0, 10(s5)
    j search_estimate
search_advance:
    sb t0, 11(s5)
search_estimate:
    # Prune when child depth + heuristic exceeds current IDA* bound.
    lhu a0, 4(s5)
    call permutation_bound
    lbu t0, 12(s5)
    bge a0, t0, search_max_ready
    mv a0, t0
search_max_ready:
    addi t0, s4, 1
    add t1, t0, a0
    blt s3, t1, search_node
    add t1, s2, s4
    sb s10, 1(t1)
    lhu t1, 4(s5)
    bnez t1, search_not_solved
    lhu t1, 6(s5)
    bnez t1, search_not_solved
    sb t0, 0(s2)
    mv a0, t0
    j search_exit
search_not_solved:
    # A non-goal at depth==bound cannot have a feasible child.
    beq t0, s3, search_node
    addi t1, s5, 16
    lhu t2, 4(s5)
    sh t2, 0(t1)
    lhu t2, 6(s5)
    sh t2, 2(t1)
    lbu t2, 12(s5)
    sb t2, 8(t1)
    sb s8, 9(t1)
    sb zero, 10(t1)
    sb zero, 11(t1)
    sb s9, 14(t1)
    addi s4, s4, 1
    j search_node
search_next_pass:
    # A whole DFS pass failed; increase bound by one, at most MAX_DEPTH.
    addi s3, s3, 1
    j search_pass
search_failed:
    li a0, -1
search_exit:
    lw s11, 12(sp)
    lw s10, 16(sp)
    lw s9, 20(sp)
    lw s8, 24(sp)
    lw s7, 28(sp)
    lw s6, 32(sp)
    lw s5, 36(sp)
    lw s4, 40(sp)
    lw s3, 44(sp)
    lw s2, 48(sp)
    lw s1, 52(sp)
    lw s0, 56(sp)
    lw ra, 60(sp)
    addi sp, sp, 64
    ret

# solve_cube(input, solution): public entry called by runner.s.
# Inputs: a0=input string, a1=solution buffer (1 length byte + 11 move bytes).
# Returns >=0 solution length, -2 for invalid input, -1 for search/replay failure.
# Replay independently checks that the generated path reaches (p,o)=(0,0).
.globl solve_cube
solve_cube:
    addi sp, sp, -48
    sw ra, 44(sp)
    sw s0, 40(sp)
    sw s1, 36(sp)
    sw s2, 32(sp)
    sw s3, 28(sp)
    sw s4, 24(sp)
    sw s5, 20(sp)
    sw s6, 16(sp)
    beqz a0, solve_invalid
    beqz a1, solve_invalid
    mv s2, a1                 # output pointer
    call parse_encode
    li t0, -2
    beq a0, t0, solve_invalid
    mv s0, a0                 # original p
    mv s1, a1                 # original o
    mv a2, s2
    call search
    bltz a0, solve_failed
    mv s3, a0                 # length
    mv s4, s0                 # replay p
    mv s5, s1                 # replay o
    li s6, 0
solve_replay:
    # Read move bytes at solution[1+i] and apply them from the original root.
    bge s6, s3, solve_verify
    add t0, s2, s6
    lbu a2, 1(t0)
    mv a0, s4
    mv a1, s5
    call apply_rank_move
    mv s4, a0
    mv s5, a1
    addi s6, s6, 1
    j solve_replay
solve_verify:
    # Both coordinates must be zero, not just the subgroup remainder.
    or t0, s4, s5
    bnez t0, solve_failed
    mv a0, s3
    j solve_exit
solve_invalid:
    li a0, -2
    j solve_exit
solve_failed:
    li a0, -1
solve_exit:
    lw s6, 16(sp)
    lw s5, 20(sp)
    lw s4, 24(sp)
    lw s3, 28(sp)
    lw s2, 32(sp)
    lw s1, 36(sp)
    lw s0, 40(sp)
    lw ra, 44(sp)
    addi sp, sp, 48
    ret
