                                                                                # int round_ppc (float x);
                                                                                .text
.globl round_ppc
round_ppc:
    fctiw %f0,%f1
    stfd %f0,-8(%r1)
    lwz %r3,-4(%r1)
    blr
