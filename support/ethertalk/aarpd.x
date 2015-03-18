/*
 * aarpd.x: RPC declarations for Appletalk ARP daemon
 * 
 */

typedef unsigned char etheraddr[6];

program AARPDPROG {
    version AARPDVERS {
	etheraddr AARP_RESOLVE(int) = 1;
    } = 1;
} = 200;
