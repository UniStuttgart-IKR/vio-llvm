.globl  memcpy
.type   memcpy,@function

memcpy:     beqz    a2, nothing
            andi    t1, a2,-4
            andi    t2, a0,-4
            andi    t3, a1,-4
            or      t1, t1,t2
            add     t6, a0,a2
            or      t1, t1,t3
            mv      t5, a0
            bnez    t1, byte_memcpy
l1:         lw      t4, 0(t5)
            addi    t5, t5,4
            sw      t4, 0(a1)
            addi    a1, a1,4
            bleu    t5,t6,l1
            ret
            

byte_memcpy:
            lbu     t4, 0(t5)
            addi    t5, t5,1
            sb      t4, 0(a1)
            addi    a1, a1,1
            bleu    t5,t6,byte_memcpy
            ret

nothing:    ret


.size   memcpy, .-memcpy