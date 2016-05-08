#ifndef X86_HOO
#define X86_HOO

void CopyInstructions(void *dest, void *src, int length);
int CountInstructionLength(void *address, int min_length);

#endif // X86_HOO
