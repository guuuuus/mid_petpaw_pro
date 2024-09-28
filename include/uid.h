#ifndef uid_h
#define uid_h
#define UID1 0x1FFFF7E8
#define UID2 0x1FFFF7EC
#define UID3 0x1FFFF7F0
unsigned long uid1()
{
    return *(unsigned long *)UID1;
}

unsigned long uid2()
{
    return *(unsigned long *)UID2;
}

unsigned long uid3()
{
    return *(unsigned long *)UID3;
}

#endif