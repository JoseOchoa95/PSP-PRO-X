/* The same pinned public CA input shipped in the manual package. No keys.
 * Resident native checking must not depend on removable storage. */
.section .rodata
.balign 4
.global prox_native_ca
.global prox_native_ca_end
prox_native_ca:
.incbin "cacert.pem"
.byte 0
prox_native_ca_end:
